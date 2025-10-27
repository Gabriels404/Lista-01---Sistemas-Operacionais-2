// ex8_buffer_bursts.c
// Buffer circular limitado com múltiplos produtores/consumidores,
// simula bursts (rajadas) e ócio, implementa backpressure (produtores aguardam)
// Grava ocupação do buffer ao longo do tempo e imprime no final.
// Windows API version.

#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0600   /* Windows Vista / Server 2008 or newer */
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>


#define DEFAULT_BUFFER 8
#define DEFAULT_PRODS 3
#define DEFAULT_CONS 2
#define SAMPLE_INTERVAL_MS 100
#define RUN_SECONDS 10

typedef struct {
    int *buf;
    int capacity;
    int head, tail, count;
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE not_empty;
    CONDITION_VARIABLE not_full;
} RingBuffer;

volatile LONG stop_flag = 0;
RingBuffer rb;
int producers = DEFAULT_PRODS;
int consumers = DEFAULT_CONS;
int samples_capacity = (RUN_SECONDS*1000)/SAMPLE_INTERVAL_MS + 10;
int *samples; int sample_pos = 0;

static double now_ms() {
    LARGE_INTEGER f, t; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t);
    return (double)t.QuadPart*1000.0/(double)f.QuadPart;
}

void rb_init(RingBuffer *r, int cap) {
    r->buf = (int*)malloc(sizeof(int)*cap);
    r->capacity = cap; r->head = r->tail = r->count = 0;
    InitializeCriticalSection(&r->cs);
    InitializeConditionVariable(&r->not_empty);
    InitializeConditionVariable(&r->not_full);
}

void rb_destroy(RingBuffer *r) {
    free(r->buf);
    DeleteCriticalSection(&r->cs);
}

void rb_put(RingBuffer *r, int item) {
    EnterCriticalSection(&r->cs);
    while (r->count == r->capacity && !stop_flag) {
        // backpressure: wait until not full
        SleepConditionVariableCS(&r->not_full, &r->cs, INFINITE);
    }
    if (stop_flag) { LeaveCriticalSection(&r->cs); return; }
    r->buf[r->tail] = item;
    r->tail = (r->tail+1)%r->capacity;
    r->count++;
    WakeConditionVariable(&r->not_empty);
    LeaveCriticalSection(&r->cs);
}

int rb_get(RingBuffer *r, int *out) {
    EnterCriticalSection(&r->cs);
    while (r->count == 0 && !stop_flag) {
        SleepConditionVariableCS(&r->not_empty, &r->cs, INFINITE);
    }
    if (r->count == 0 && stop_flag) { LeaveCriticalSection(&r->cs); return 0; }
    *out = r->buf[r->head];
    r->head = (r->head+1)%r->capacity;
    r->count--;
    WakeConditionVariable(&r->not_full);
    LeaveCriticalSection(&r->cs);
    return 1;
}

DWORD WINAPI producer(LPVOID arg) {
    int id = (int)(intptr_t)arg;
    int burst_chance = 20; // % chance to start a burst
    while (!stop_flag) {
        // decide burst or idle
        int r = rand()%100;
        if (r < burst_chance) {
            // burst: produce many quickly
            int burst_len = 2 + rand()%5;
            for (int i=0;i<burst_len && !stop_flag;i++) {
                rb_put(&rb, id*1000 + rand()%1000);
                Sleep(10 + rand()%20); // quick
            }
        } else {
            // idle: produce rarely
            rb_put(&rb, id*1000 + rand()%1000);
            Sleep(150 + rand()%300);
        }
    }
    return 0;
}

DWORD WINAPI consumer(LPVOID arg) {
    int id = (int)(intptr_t)arg;
    while (!stop_flag) {
        int item;
        if (!rb_get(&rb, &item)) break;
        // process
        Sleep(50 + rand()%100);
        //printf("C%d consumed %d\n", id, item);
    }
    return 0;
}

DWORD WINAPI sampler(LPVOID arg) {
    (void)arg;
    while (!stop_flag) {
        EnterCriticalSection(&rb.cs);
        int occ = rb.count;
        LeaveCriticalSection(&rb.cs);
        if (sample_pos < samples_capacity) samples[sample_pos++] = occ;
        Sleep(SAMPLE_INTERVAL_MS);
    }
    return 0;
}

int main(int argc, char** argv) {
    int bufsize = DEFAULT_BUFFER;
    if (argc >= 2) bufsize = atoi(argv[1])>1?atoi(argv[1]):DEFAULT_BUFFER;
    if (argc >= 3) producers = atoi(argv[2])>0?atoi(argv[2]):DEFAULT_PRODS;
    if (argc >= 4) consumers = atoi(argv[3])>0?atoi(argv[3]):DEFAULT_CONS;

    srand((unsigned)time(NULL));
    samples = (int*)malloc(sizeof(int)*(samples_capacity+10));
    rb_init(&rb, bufsize);

    HANDLE *pth_prod = malloc(sizeof(HANDLE)*producers);
    HANDLE *pth_cons = malloc(sizeof(HANDLE)*consumers);

    // start consumers
    for (int i=0;i<consumers;i++) pth_cons[i] = CreateThread(NULL,0,consumer,(LPVOID)(intptr_t)i,0,NULL);
    // start producers
    for (int i=0;i<producers;i++) pth_prod[i] = CreateThread(NULL,0,producer,(LPVOID)(intptr_t)i,0,NULL);
    HANDLE pSampler = CreateThread(NULL,0,sampler,NULL,0,NULL);

    Sleep(RUN_SECONDS*1000);
    InterlockedExchange(&stop_flag, 1);
    // wake all waiting threads
    WakeAllConditionVariable(&rb.not_empty);
    WakeAllConditionVariable(&rb.not_full);

    WaitForMultipleObjects(producers, pth_prod, TRUE, INFINITE);
    WaitForMultipleObjects(consumers, pth_cons, TRUE, INFINITE);
    WaitForSingleObject(pSampler, INFINITE);

    // print occupancy timeline
    printf("Buffer occupancy samples (most recent first):\n");
    for (int i=0;i<sample_pos;i++) {
        printf("%d ", samples[i]);
    }
    printf("\nAverage occupancy: ");
    double sum=0;
    for (int i=0;i<sample_pos;i++) sum += samples[i];
    printf("%.2f / %d capacity\n", sample_pos>0 ? sum/sample_pos : 0.0, rb.capacity);

    // cleanup
    for (int i=0;i<producers;i++) CloseHandle(pth_prod[i]);
    for (int i=0;i<consumers;i++) CloseHandle(pth_cons[i]);
    CloseHandle(pSampler);
    rb_destroy(&rb);
    free(pth_prod); free(pth_cons); free(samples);
    return 0;
}
