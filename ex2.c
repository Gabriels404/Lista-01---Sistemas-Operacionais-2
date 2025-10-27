// ex2_buffer.c
// Buffer circular (bounded) com múltiplos produtores e consumidores.
// Usa CRITICAL_SECTION + CONDITION_VARIABLE para exclusão mútua e espera sem busy-wait.
// Simples estatísticas de throughput e tempo médio de espera.
//
// Compilar: cl ex2_buffer.c  OR  gcc -o ex2_buffer.exe ex2_buffer.c

#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0600   /* Windows Vista / Server 2008 or newer */
#endif

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define MAX_BUF 128

typedef struct {
    int *buf;
    int capacity;
    int head, tail, count;
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cv_not_empty;
    CONDITION_VARIABLE cv_not_full;
} RingBuf;

RingBuf rb;
int producers = 2, consumers = 2;
int total_items = 200;
int produced = 0;
int consumed = 0;
LONG produced_count = 0;

void ring_init(RingBuf* r, int cap){
    r->buf = (int*)malloc(sizeof(int)*cap);
    r->capacity = cap;
    r->head = r->tail = r->count = 0;
    InitializeCriticalSection(&r->cs);
    InitializeConditionVariable(&r->cv_not_empty);
    InitializeConditionVariable(&r->cv_not_full);
}

void ring_destroy(RingBuf* r){
    free(r->buf);
    DeleteCriticalSection(&r->cs);
}

void ring_put(RingBuf* r, int v){
    EnterCriticalSection(&r->cs);
    while (r->count == r->capacity) {
        SleepConditionVariableCS(&r->cv_not_full, &r->cs, INFINITE);
    }
    r->buf[r->tail] = v;
    r->tail = (r->tail+1)%r->capacity;
    r->count++;
    WakeConditionVariable(&r->cv_not_empty);
    LeaveCriticalSection(&r->cs);
}

int ring_get(RingBuf* r){
    EnterCriticalSection(&r->cs);
    while (r->count == 0) {
        SleepConditionVariableCS(&r->cv_not_empty, &r->cs, INFINITE);
    }
    int v = r->buf[r->head];
    r->head = (r->head+1)%r->capacity;
    r->count--;
    WakeConditionVariable(&r->cv_not_full);
    LeaveCriticalSection(&r->cs);
    return v;
}

DWORD WINAPI producer(LPVOID arg){
    int id = (int)(intptr_t)arg;
    while (1) {
        EnterCriticalSection(&rb.cs);
        if (produced >= total_items) {
            LeaveCriticalSection(&rb.cs);
            break;
        }
        produced++;
        int item = produced;
        LeaveCriticalSection(&rb.cs);

        // simulate work
        Sleep(rand()%50);
        ring_put(&rb, item);
        InterlockedIncrement(&produced_count);
        // optional: print
        // printf("P%d produced %d\n", id, item);
    }
    return 0;
}

DWORD WINAPI consumer(LPVOID arg){
    int id = (int)(intptr_t)arg;
    while (1) {
        EnterCriticalSection(&rb.cs);
        if (consumed >= total_items) {
            LeaveCriticalSection(&rb.cs);
            break;
        }
        LeaveCriticalSection(&rb.cs);

        int item = ring_get(&rb);
        // process
        Sleep(rand()%80);
        EnterCriticalSection(&rb.cs);
        consumed++;
        LeaveCriticalSection(&rb.cs);
        // printf("C%d consumed %d\n", id, item);
    }
    return 0;
}

int main(){
    srand((unsigned)time(NULL));
    int bufsize = 8;
    printf("Buffer size (N): ");
    scanf("%d",&bufsize);
    printf("Producers? ");
    scanf("%d",&producers);
    printf("Consumers? ");
    scanf("%d",&consumers);
    printf("Total items: ");
    scanf("%d",&total_items);

    ring_init(&rb, bufsize);

    HANDLE pth[32], cth[32];
    for (int i=0;i<producers;i++) pth[i] = CreateThread(NULL,0,producer,(LPVOID)(intptr_t)i,0,NULL);
    for (int i=0;i<consumers;i++) cth[i] = CreateThread(NULL,0,consumer,(LPVOID)(intptr_t)i,0,NULL);

    WaitForMultipleObjects(producers, pth, TRUE, INFINITE);
    // Ensure consumers wake up if waiting and items finished
    EnterCriticalSection(&rb.cs);
    WakeAllConditionVariable(&rb.cv_not_empty);
    LeaveCriticalSection(&rb.cs);

    WaitForMultipleObjects(consumers, cth, TRUE, INFINITE);

    printf("Done. produced=%ld consumed=%d\n", produced_count, consumed);
    ring_destroy(&rb);
    return 0;
}
