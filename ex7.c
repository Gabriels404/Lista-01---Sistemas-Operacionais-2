// ex7_filosofos.c
// Problema dos filósofos - duas soluções:
// 1) ordem global de aquisição (pegar o garfo de menor índice primeiro)
// 2) limitar simultâneos com um semáforo (N-1 solução classical)
// Compila no Windows (MinGW / MSVC)
// Uso: ex7_filosofos.exe [N_filosofo] [modo]
// modo: 1 = ordem global (default), 2 = semaforo limitador
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define DEFAULT_N 5
#define RUN_SECONDS 10

typedef struct {
    int id;
    int meals;
    double max_wait_ms;
    DWORD thread_id;
} Philosopher;

CRITICAL_SECTION *forks; // array of mutexes (CRITICAL_SECTION)
HANDLE limiter = NULL;   // semaphore for mode 2
Philosopher *ph;
int N = DEFAULT_N;
int mode = 1;
volatile LONG stop_flag = 0;

// timing helper
static double now_ms() {
    static LARGE_INTEGER freq;
    static int inited = 0;
    if (!inited) { QueryPerformanceFrequency(&freq); inited = 1; }
    LARGE_INTEGER t; QueryPerformanceCounter(&t);
    return (double)t.QuadPart * 1000.0 / (double)freq.QuadPart;
}

DWORD WINAPI philosopher_thread(LPVOID arg) {
    Philosopher *p = (Philosopher*)arg;
    int left = p->id;
    int right = (p->id + 1) % N;
    while (!stop_flag) {
        // think
        Sleep( (rand() % 50) + 20 );

        double t0 = now_ms();

        if (mode == 1) {
            // ordem global: adquira primeiro o garfo de menor índice
            int first = left < right ? left : right;
            int second = left < right ? right : left;
            EnterCriticalSection(&forks[first]);
            EnterCriticalSection(&forks[second]);
        } else {
            // modo 2: aguarda semaforo (N-1) e pega ambos (ordem arbitraria)
            WaitForSingleObject(limiter, INFINITE);
            EnterCriticalSection(&forks[left]);
            EnterCriticalSection(&forks[right]);
        }

        double waited = now_ms() - t0;
        if (waited > p->max_wait_ms) p->max_wait_ms = waited;

        // eat
        p->meals++;
        Sleep( (rand() % 80) + 20 );

        // release
        LeaveCriticalSection(&forks[left]);
        LeaveCriticalSection(&forks[right]);
        if (mode == 2) ReleaseSemaphore(limiter, 1, NULL);

        // short rest
        Sleep( (rand() % 50) + 10 );
    }
    return 0;
}

int main(int argc, char** argv) {
    srand((unsigned)time(NULL));
    if (argc >= 2) N = atoi(argv[1]) > 1 ? atoi(argv[1]) : DEFAULT_N;
    if (argc >= 3) mode = atoi(argv[2]) == 2 ? 2 : 1;

    printf("Filósofos N=%d, modo=%d (%s)\n", N, mode, mode==1?"ordem global":"semáforo limitador (N-1)");
    forks = (CRITICAL_SECTION*)malloc(sizeof(CRITICAL_SECTION) * N);
    ph = (Philosopher*)malloc(sizeof(Philosopher) * N);
    for (int i=0;i<N;i++) InitializeCriticalSection(&forks[i]);
    if (mode == 2) limiter = CreateSemaphore(NULL, N-1, N-1, NULL);

    HANDLE *ths = (HANDLE*)malloc(sizeof(HANDLE)*N);
    for (int i=0;i<N;i++) {
        ph[i].id = i;
        ph[i].meals = 0;
        ph[i].max_wait_ms = 0.0;
        ths[i] = CreateThread(NULL,0,philosopher_thread,&ph[i],0,NULL);
    }

    Sleep(RUN_SECONDS * 1000);
    InterlockedExchange(&stop_flag, 1);
    WaitForMultipleObjects(N, ths, TRUE, INFINITE);

    printf("\nResultados por filósofo:\n");
    for (int i=0;i<N;i++) {
        printf("Philosopher %d: meals=%d, max_wait=%.3f ms\n", i, ph[i].meals, ph[i].max_wait_ms);
    }

    // cleanup
    for (int i=0;i<N;i++) DeleteCriticalSection(&forks[i]);
    if (mode==2) CloseHandle(limiter);
    free(forks); free(ph); free(ths);
    return 0;
}
