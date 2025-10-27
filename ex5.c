// ex5_threadpool.c
// Pool fixo de N threads (Win32) que processa tarefas CPU-bound: Fibonacci iterativo.
// Lê tarefas da entrada padrão até EOF (linhas "fib <n>"), enfileira e processa.
// Finaliza corretamente com sinalização.
// Simples, sem dependências externas.
//
// Compilar: cl ex5_threadpool.c  OR  gcc -o ex5_threadpool.exe ex5_threadpool.c

#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0600   /* Windows Vista / Server 2008 or newer */
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

typedef struct Task {
    long long id;
    long long n;
    struct Task* next;
} Task;

CRITICAL_SECTION qcs;
CONDITION_VARIABLE qcv;
Task* qhead=NULL;
Task* qtail=NULL;
int shutdown_flag = 0;
LONG enqueued=0, processed=0;

unsigned long long fib_iter(long long n){
    if (n<=0) return 0;
    if (n==1) return 1;
    unsigned long long a=0,b=1;
    for (long long i=2;i<=n;i++){ unsigned long long c=a+b; a=b; b=c; }
    return b;
}

void enqueue(Task* t){
    EnterCriticalSection(&qcs);
    t->next = NULL;
    if (!qtail) qhead = qtail = t;
    else { qtail->next = t; qtail = t; }
    WakeConditionVariable(&qcv);
    LeaveCriticalSection(&qcs);
}
Task* dequeue(){
    EnterCriticalSection(&qcs);
    Task* t = qhead;
    if (t){
        qhead = t->next;
        if (!qhead) qtail = NULL;
    }
    LeaveCriticalSection(&qcs);
    return t;
}

DWORD WINAPI worker(LPVOID arg){
    int id = (int)(intptr_t)arg;
    for(;;){
        EnterCriticalSection(&qcs);
        while (!qhead && !shutdown_flag) SleepConditionVariableCS(&qcv,&qcs,INFINITE);
        if (!qhead && shutdown_flag) { LeaveCriticalSection(&qcs); break; }
        Task* t = dequeue();
        LeaveCriticalSection(&qcs);
        if (t){
            unsigned long long res = fib_iter(t->n);
            EnterCriticalSection(&qcs);
            printf("[W%d] id=%lld fib(%lld)=%llu\n", id, t->id, t->n, res);
            LeaveCriticalSection(&qcs);
            InterlockedIncrement(&processed);
            free(t);
        }
    }
    return 0;
}

int main(int argc, char** argv){
    InitializeCriticalSection(&qcs);
    InitializeConditionVariable(&qcv);

    int nthreads = 4;
    if (argc>1) nthreads = atoi(argv[1]);
    HANDLE *ths = malloc(sizeof(HANDLE)*nthreads);
    for (int i=0;i<nthreads;i++) ths[i]=CreateThread(NULL,0,worker,(LPVOID)(intptr_t)i,0,NULL);

    // read stdin
    char line[128];
    long long next_id=1;
    while (fgets(line,sizeof(line),stdin)){
        char cmd[16]; long long n;
        if (sscanf(line,"%15s %lld",cmd,&n)==2 && strcmp(cmd,"fib")==0){
            Task* t = malloc(sizeof(Task));
            t->id = next_id++; t->n = n; t->next = NULL;
            enqueue(t);
            InterlockedIncrement(&enqueued);
        } else {
            printf("Invalid. Use: fib <n>\n");
        }
    }

    // shutdown: wait until queue empty then signal shutdown
    for(;;){
        EnterCriticalSection(&qcs);
        int empty = (qhead==NULL);
        if (empty){ shutdown_flag = 1; WakeAllConditionVariable(&qcv); LeaveCriticalSection(&qcs); break; }
        LeaveCriticalSection(&qcs);
        Sleep(50);
    }

    WaitForMultipleObjects(nthreads, ths, TRUE, INFINITE);
    for (int i=0;i<nthreads;i++) CloseHandle(ths[i]);
    printf("Enqueued=%ld Processed=%ld\n", enqueued, processed);
    DeleteCriticalSection(&qcs);
    free(ths);
    return 0;
}
