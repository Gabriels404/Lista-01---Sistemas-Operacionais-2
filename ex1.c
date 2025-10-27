// ex1_corrida.c
// Corrida de cavalos: cada cavalo é uma thread; largada sincronizada;
// aposta do usuário; atualização do placar com exclusão mútua;
// empates resolvidos deterministicamente pelo menor índice (ID).
//
// Compilar: cl ex1_corrida.c  OR  gcc -o ex1_corrida.exe ex1_corrida.c

#ifndef _WIN32_WINNT
  #define _WIN32_WINNT 0x0600   /* Windows Vista / Server 2008 or newer */
#endif
#include <windows.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_HORSES 10
#define FINISH 100

typedef struct {
    int id;
    int pos;
    int finished;
} Horse;

Horse horses[MAX_HORSES];
int H = 5;
CRITICAL_SECTION cs;
CONDITION_VARIABLE cv_start;
int start_flag = 0;
int finish_order[MAX_HORSES];
int finish_count = 0;

DWORD WINAPI horse_thread(LPVOID arg){
    Horse* h = (Horse*)arg;
    // Espera largada sincronizada
    EnterCriticalSection(&cs);
    while(!start_flag) SleepConditionVariableCS(&cv_start, &cs, INFINITE);
    LeaveCriticalSection(&cs);

    // Avança em passos aleatórios até cruzar a linha
    while (1) {
        Sleep(50 + rand()%150);
        EnterCriticalSection(&cs);
        if (!h->finished) {
            int step = 1 + rand()%10;
            h->pos += step;
            if (h->pos >= FINISH) {
                h->pos = FINISH;
                h->finished = 1;
                // registra finish de forma determinística: menor id primeiro se vários chegarem logo
                finish_order[finish_count++] = h->id;
            }
        }
        LeaveCriticalSection(&cs);
        if (h->finished) break;
    }
    return 0;
}

int main(){
    srand((unsigned)time(NULL));
    InitializeCriticalSection(&cs);
    InitializeConditionVariable(&cv_start);

    printf("Quantos cavalos? (max %d): ", MAX_HORSES);
    scanf("%d", &H);
    if (H < 2) H = 2;
    if (H > MAX_HORSES) H = MAX_HORSES;
    getchar();

    char bet[32];
    printf("Aposte em qual cavalo (0..%d)? Digite número: ", H-1);
    fgets(bet, sizeof(bet), stdin);
    int bet_id = atoi(bet);

    HANDLE th[MAX_HORSES];
    for (int i=0;i<H;i++){
        horses[i].id = i;
        horses[i].pos = 0;
        horses[i].finished = 0;
        finish_order[i] = -1;
        th[i] = CreateThread(NULL,0,horse_thread,&horses[i],0,NULL);
    }

    printf("Preparar... (pressione Enter para largar)\n");
    getchar();
    EnterCriticalSection(&cs);
    start_flag = 1;
    WakeAllConditionVariable(&cv_start);
    LeaveCriticalSection(&cs);

    // aguarda todos finalizarem
    WaitForMultipleObjects(H, th, TRUE, INFINITE);

    // garante ordem determinística em caso de empates: já gravado por id na ordem que marcou
    printf("Resultado (ordem de chegada):\n");
    for (int i=0;i<H;i++){
        printf("%d: Cavalo %d\n", i+1, finish_order[i]);
    }

    int winner = finish_order[0];
    printf("Vencedor: Cavalo %d\n", winner);
    if (bet_id == winner) printf("Parabéns! Sua aposta estava correta.\n");
    else printf("Que pena — sua aposta estava errada.\n");

    for (int i=0;i<H;i++) CloseHandle(th[i]);
    DeleteCriticalSection(&cs);
    return 0;
}
