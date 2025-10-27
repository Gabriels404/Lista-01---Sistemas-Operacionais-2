// ex3_transferencias.c
// Simula M contas e T threads que fazem transferências aleatórias entre contas.
// Protege saldos com mutex por conta; verifica (asserção) que soma total permanece constante.
// Também proporciona uma execução "sem trava" para evidenciar condição de corrida.
//
// Compilar: cl ex3_transferencias.c  OR  gcc -o ex3_transferencias.exe ex3_transferencias.c

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>

#define MAX_ACCOUNTS 64

double balances[MAX_ACCOUNTS];
CRITICAL_SECTION acc_cs[MAX_ACCOUNTS];
int M = 8; // contas
int T = 4; // threads
int ops_per_thread = 10000;
int use_locks = 1;

DWORD WINAPI transfer_thread(LPVOID arg){
    unsigned int seed = (unsigned int)GetTickCount() ^ (unsigned int)(intptr_t)arg;
    for (int k=0;k<ops_per_thread;k++){
        int a = rand_r(&seed) % M;
        int b = rand_r(&seed) % M;
        double amount = (rand_r(&seed) % 1000) / 100.0;
        if (a==b) continue;

        if (use_locks){
            // lock ordering to prevent deadlock: lock smaller index first
            int first = a < b ? a : b;
            int second = a < b ? b : a;
            EnterCriticalSection(&acc_cs[first]);
            EnterCriticalSection(&acc_cs[second]);

            if (balances[a] >= amount){
                balances[a] -= amount;
                balances[b] += amount;
            }

            LeaveCriticalSection(&acc_cs[second]);
            LeaveCriticalSection(&acc_cs[first]);
        } else {
            // no locks - race condition likely
            if (balances[a] >= amount){
                balances[a] -= amount;
                balances[b] += amount;
            }
        }
    }
    return 0;
}

double total_balance(){
    double s=0;
    for (int i=0;i<M;i++) s += balances[i];
    return s;
}

int main(){
    srand((unsigned)time(NULL));
    printf("Contas (M) ? "); scanf("%d",&M);
    printf("Threads (T) ? "); scanf("%d",&T);
    printf("Ops por thread ? "); scanf("%d",&ops_per_thread);
    printf("Usar locks? (1=sim,0=nao) ? "); scanf("%d",&use_locks);

    for (int i=0;i<M;i++){
        balances[i] = 1000.0; // saldo inicial
        InitializeCriticalSection(&acc_cs[i]);
    }
    double initial = total_balance();
    printf("Soma inicial: %.2f\n", initial);

    HANDLE th[128];
    for (int i=0;i<T;i++) th[i] = CreateThread(NULL,0,transfer_thread,(LPVOID)(intptr_t)i,0,NULL);
    WaitForMultipleObjects(T, th, TRUE, INFINITE);

    double final = total_balance();
    printf("Soma final: %.2f\n", final);
    if (fabs(final - initial) > 0.01) {
        printf("ASSERT FAIL: soma global mudou! (condição de corrida provavelmente)\n");
    } else {
        printf("OK: soma global preservada.\n");
    }
    for (int i=0;i<M;i++) DeleteCriticalSection(&acc_cs[i]);
    return 0;
}
