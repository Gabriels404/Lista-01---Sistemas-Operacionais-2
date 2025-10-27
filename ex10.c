// ex10_deadlock_watchdog.c
// Cria threads que tentam adquirir dois recursos em ordens distintas (possível deadlock).
// Uma thread watchdog detecta ausência de progresso por T segundos e reporta.
// Depois demonstra correção adotando ordem total de travamento.
// Windows API version.
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define RESOURCES 3
#define THREADS 6
#define WATCHDOG_TIMEOUT_MS 2000
#define RUN_MS 5000

CRITICAL_SECTION resources[RESOURCES];
volatile LONG last_progress_ms = 0; // updated on each successful lock/unlock action
volatile LONG stop_flag = 0;

static double now_ms() {
    LARGE_INTEGER f,t; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t);
    return (double)t.QuadPart*1000.0/(double)f.QuadPart;
}

DWORD WINAPI worker_deadlock_prone(LPVOID arg) {
    int id = (int)(intptr_t)arg;
    // pick two distinct resources
    int a = rand()%RESOURCES;
    int b = rand()%RESOURCES;
    while (b == a) b = rand()%RESOURCES;
    int order = rand()%2; // 0 => a then b, 1 => b then a

    while (!stop_flag) {
        // try to acquire in random order -> may deadlock with others
        if (order == 0) {
            EnterCriticalSection(&resources[a]);
            // simulate some work
            Sleep(10 + rand()%30);
            EnterCriticalSection(&resources[b]);
        } else {
            EnterCriticalSection(&resources[b]);
            Sleep(10 + rand()%30);
            EnterCriticalSection(&resources[a]);
        }
        // critical section
        InterlockedExchange(&last_progress_ms, (LONG)now_ms());
        Sleep(20 + rand()%30);

        // release
        LeaveCriticalSection(&resources[a]);
        LeaveCriticalSection(&resources[b]);
        InterlockedExchange(&last_progress_ms, (LONG)now_ms());

        Sleep(50 + rand()%100);
    }
    return 0;
}

DWORD WINAPI watchdog_thread(LPVOID arg) {
    (void)arg;
    while (!stop_flag) {
        Sleep(500);
        LONG lp = last_progress_ms;
        double diff = now_ms() - (double)lp;
        if (lp == 0 || diff > WATCHDOG_TIMEOUT_MS) {
            printf("[WATCHDOG] No progress detected in %.0f ms -> possible deadlock/stall\n", diff);
            // Try to report which threads might be stuck by listing resource owners is complex in Win32 CRITICAL_SECTION.
            // Here we just advise that deadlock likely happened.
            // In a richer implementation we'd use TryEnterCriticalSection to probe resource ownership.
            // Let's probe resources with TryEnterCriticalSection to see which are free/locked.
            for (int r=0;r<RESOURCES;r++) {
                if (TryEnterCriticalSection(&resources[r])) {
                    // we acquired it -> it was free
                    LeaveCriticalSection(&resources[r]);
                    printf("  Resource %d: FREE\n", r);
                } else {
                    printf("  Resource %d: LOCKED (someone holds it)\n", r);
                }
            }
            // reset lp baseline so repeated messages not too spammy
            InterlockedExchange(&last_progress_ms, (LONG)now_ms());
        }
    }
    return 0;
}

// Fixed version: enforce global resource ordering a < b < c when acquiring multiple resources.
DWORD WINAPI worker_fixed(LPVOID arg) {
    int id = (int)(intptr_t)arg;
    int a = rand()%RESOURCES;
    int b = rand()%RESOURCES;
    while (b==a) b = rand()%RESOURCES;
    int first = a < b ? a : b;
    int second = a < b ? b : a;
    while (!stop_flag) {
        EnterCriticalSection(&resources[first]);
        Sleep(5 + rand()%20);
        EnterCriticalSection(&resources[second]);

        InterlockedExchange(&last_progress_ms, (LONG)now_ms());
        Sleep(15 + rand()%25);

        LeaveCriticalSection(&resources[second]);
        LeaveCriticalSection(&resources[first]);
        InterlockedExchange(&last_progress_ms, (LONG)now_ms());

        Sleep(40 + rand()%80);
    }
    return 0;
}

int main(int argc, char** argv) {
    srand((unsigned)time(NULL));
    for (int i=0;i<RESOURCES;i++) InitializeCriticalSection(&resources[i]);

    printf("Fase 1: executando versão propensa a deadlock por %d ms...\n", RUN_MS);
    HANDLE ths[THREADS];
    for (int i=0;i<THREADS;i++) ths[i] = CreateThread(NULL,0,worker_deadlock_prone,(LPVOID)(intptr_t)i,0,NULL);
    HANDLE wd = CreateThread(NULL,0,watchdog_thread,NULL,0,NULL);
    Sleep(RUN_MS);
    InterlockedExchange(&stop_flag, 1);
    WaitForMultipleObjects(THREADS, ths, TRUE, INFINITE);
    WaitForSingleObject(wd, INFINITE);

    // reset
    InterlockedExchange(&stop_flag, 0);
    InterlockedExchange(&last_progress_ms, (LONG)now_ms());
    printf("\nFase 2: executando versão FIX (ordem total de travamento) por %d ms...\n", RUN_MS);
    for (int i=0;i<THREADS;i++) ths[i] = CreateThread(NULL,0,worker_fixed,(LPVOID)(intptr_t)i,0,NULL);
    wd = CreateThread(NULL,0,watchdog_thread,NULL,0,NULL);
    Sleep(RUN_MS);
    InterlockedExchange(&stop_flag, 1);
    WaitForMultipleObjects(THREADS, ths, TRUE, INFINITE);
    WaitForSingleObject(wd, INFINITE);

    printf("Terminado. (Se a versão 1 mostrou watchdog ativo, havia perda de progresso.)\n");
    for (int i=0;i<RESOURCES;i++) DeleteCriticalSection(&resources[i]);
    return 0;
}
