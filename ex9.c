// ex9_revezamento.c
// Corrida de revezamento: K threads por equipe; todas as threads da equipe
// devem alcançar uma barreira para liberar a próxima perna.
// Implementa barreira com mutex+condvar e mede rodadas por minuto.
// Compila no Windows.
// Uso: ex9_revezamento.exe [teams] [K_por_team] [duration_seconds]

#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0600   /* Windows Vista / Server 2008 or newer */
#endif
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

typedef struct {
    int team;
    int id;
} RunnerArg;

typedef struct {
    int count; // number arrived
    int threshold;
    CRITICAL_SECTION cs;
    CONDITION_VARIABLE cv;
} Barrier;

volatile LONG stop_flag = 0;
Barrier *barriers; // one barrier per team
int teams = 2;
int K = 3;
int duration_seconds = 15;
int *rounds_completed; // per team

static double now_ms() { LARGE_INTEGER f,t; QueryPerformanceFrequency(&f); QueryPerformanceCounter(&t); return (double)t.QuadPart*1000.0/(double)f.QuadPart; }

void barrier_init(Barrier *b, int thr) {
    b->count = 0; b->threshold = thr;
    InitializeCriticalSection(&b->cs);
    InitializeConditionVariable(&b->cv);
}

void barrier_wait(Barrier *b) {
    EnterCriticalSection(&b->cs);
    b->count++;
    if (b->count >= b->threshold) {
        b->count = 0; // reset for next round
        WakeAllConditionVariable(&b->cv);
    } else {
        SleepConditionVariableCS(&b->cv, &b->cs, INFINITE);
    }
    LeaveCriticalSection(&b->cs);
}

DWORD WINAPI runner_thread(LPVOID arg) {
    RunnerArg *ra = (RunnerArg*)arg;
    int team = ra->team;
    while (!stop_flag) {
        // simulate running leg
        Sleep(100 + rand()%200);
        // reach barrier
        barrier_wait(&barriers[team]);
        // only one thread per team will increment rounds -- pick thread id==0
        if (ra->id == 0) rounds_completed[team]++;
        // small rest
        Sleep(20);
    }
    return 0;
}

int main(int argc, char** argv) {
    srand((unsigned)time(NULL));
    if (argc >= 2) teams = atoi(argv[1])>0?atoi(argv[1]):teams;
    if (argc >= 3) K = atoi(argv[2])>0?atoi(argv[2]):K;
    if (argc >= 4) duration_seconds = atoi(argv[3])>0?atoi(argv[3]):duration_seconds;

    printf("Revezamento: %d equipes, %d corredores por equipe, duracao %d s\n", teams, K, duration_seconds);
    barriers = (Barrier*)malloc(sizeof(Barrier)*teams);
    rounds_completed = (int*)calloc(teams, sizeof(int));
    HANDLE *threads = (HANDLE*)malloc(sizeof(HANDLE)*teams*K);
    RunnerArg *args = (RunnerArg*)malloc(sizeof(RunnerArg)*teams*K);

    for (int t=0;t<teams;t++) barrier_init(&barriers[t], K);

    int idx = 0;
    for (int t=0;t<teams;t++) {
        for (int i=0;i<K;i++) {
            args[idx].team = t;
            args[idx].id = i;
            threads[idx] = CreateThread(NULL,0,runner_thread,&args[idx],0,NULL);
            idx++;
        }
    }

    Sleep(duration_seconds * 1000);
    InterlockedExchange(&stop_flag, 1);
    WaitForMultipleObjects(teams*K, threads, TRUE, INFINITE);

    printf("\nResultados (rodadas completadas):\n");
    for (int t=0;t<teams;t++) {
        printf("Team %d: rounds=%d (rpm ~= %.2f)\n", t, rounds_completed[t], (rounds_completed[t]/(double)duration_seconds)*60.0);
    }

    // cleanup
    free(barriers); free(rounds_completed); free(threads); free(args);
    return 0;
}
