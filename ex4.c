// ex4_linha_processamento.c
// Pipeline com 3 estágios: captura -> processamento -> gravação.
// Usa duas filas limitadas (ring buffers) protegidas com CRITICAL_SECTION + CONDITION_VARIABLE.
// Usa "poison pill" (-1) para sinalizar finalização limpa.
//
// Compilar: cl ex4_linha_processamento.c  OR  gcc -o ex4_linha_processamento.exe ex4_linha_processamento.c

#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0600   /* Windows Vista / Server 2008 or newer */
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define BUF1 8
#define BUF2 8
#define N_ITEMS 50

typedef struct {
    int *buf; int cap; int head, tail, cnt;
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE not_empty, not_full;
} Ring;

void ring_init(Ring* r, int cap){
    r->buf = malloc(sizeof(int)*cap);
    r->cap = cap; r->head=r->tail=r->cnt=0;
    InitializeCriticalSection(&r->cs);
    InitializeConditionVariable(&r->not_empty);
    InitializeConditionVariable(&r->not_full);
}
void ring_put(Ring* r, int v){
    EnterCriticalSection(&r->cs);
    while(r->cnt==r->cap) SleepConditionVariableCS(&r->not_full,&r->cs,INFINITE);
    r->buf[r->tail]=v; r->tail=(r->tail+1)%r->cap; r->cnt++;
    WakeConditionVariable(&r->not_empty);
    LeaveCriticalSection(&r->cs);
}
int ring_get(Ring* r){
    EnterCriticalSection(&r->cs);
    while(r->cnt==0) SleepConditionVariableCS(&r->not_empty,&r->cs,INFINITE);
    int v = r->buf[r->head]; r->head=(r->head+1)%r->cap; r->cnt--;
    WakeConditionVariable(&r->not_full);
    LeaveCriticalSection(&r->cs);
    return v;
}

Ring q1, q2;

DWORD WINAPI capture_thread(LPVOID arg){
    for (int i=0;i<N_ITEMS;i++){
        Sleep(rand()%50);
        printf("Captured %d\n", i);
        ring_put(&q1, i);
    }
    // poison pill for next stage
    ring_put(&q1, -1);
    return 0;
}

DWORD WINAPI process_thread(LPVOID arg){
    while (1){
        int v = ring_get(&q1);
        if (v == -1) {
            // forward poison pill
            ring_put(&q2, -1);
            break;
        }
        Sleep(50 + rand()%100);
        int processed = v*2;
        printf("Processed %d -> %d\n", v, processed);
        ring_put(&q2, processed);
    }
    return 0;
}

DWORD WINAPI writer_thread(LPVOID arg){
    while (1){
        int v = ring_get(&q2);
        if (v == -1) break;
        // simulate write
        Sleep(rand()%30);
        printf("Wrote %d\n", v);
    }
    return 0;
}

int main(){
    srand((unsigned)time(NULL));
    ring_init(&q1, BUF1);
    ring_init(&q2, BUF2);

    HANDLE t1 = CreateThread(NULL,0,capture_thread,NULL,0,NULL);
    HANDLE t2 = CreateThread(NULL,0,process_thread,NULL,0,NULL);
    HANDLE t3 = CreateThread(NULL,0,writer_thread,NULL,0,NULL);

    WaitForSingleObject(t1, INFINITE);
    WaitForSingleObject(t2, INFINITE);
    // ensure writer can finish
    WaitForSingleObject(t3, INFINITE);

    printf("Pipeline finished.\n");
    return 0;
}
