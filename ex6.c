// ex6_mapreduce.c
// Lê um arquivo grande de inteiros (um por linha) e calcula soma total + histograma
// usando P threads. Arquivo particionado em blocos (offsets) e cada thread faz "map" local.
// A redução é feita pela thread principal com exclusão mínima.
//
// Compilar: cl ex6_mapreduce.c  OR  gcc -o ex6_mapreduce.exe ex6_mapreduce.c
// Uso: ex6_mapreduce.exe arquivo.txt P
// Observação: implementado de forma simples sem mmap (fácil e portátil no Windows).

#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    char *filename;
    long start_line;
    long end_line; // inclusive
    long long partial_sum;
    int *hist; int hist_bins;
    int id;
} WorkerArg;

int P = 4;
int HIST_BINS = 10;
WorkerArg *args;

DWORD WINAPI worker(LPVOID param){
    WorkerArg* a = (WorkerArg*)param;
    FILE *f = fopen(a->filename,"r");
    if (!f) return 0;
    char buf[256];
    long line = 0;
    while (fgets(buf,sizeof(buf),f)){
        if (line > a->end_line) break;
        if (line >= a->start_line){
            long v = atol(buf);
            a->partial_sum += v;
            int bin = (v >= 0 ? v % a->hist_bins : (-v) % a->hist_bins);
            a->hist[bin]++;
        }
        line++;
    }
    fclose(f);
    return 0;
}

int main(int argc, char** argv){
    if (argc < 3){
        printf("Usage: %s arquivo.txt P\n", argv[0]); return 1;
    }
    char *filename = argv[1];
    P = atoi(argv[2]); if (P<=0) P=1;

    // conta linhas
    FILE *f = fopen(filename,"r");
    if (!f){ perror("fopen"); return 1; }
    char buf[256];
    long total_lines = 0;
    while (fgets(buf,sizeof(buf),f)) total_lines++;
    fclose(f);

    args = malloc(sizeof(WorkerArg)*P);
    HANDLE *ths = malloc(sizeof(HANDLE)*P);

    long per = total_lines / P;
    for (int i=0;i<P;i++){
        args[i].filename = filename;
        args[i].start_line = i*per;
        args[i].end_line = (i==P-1) ? (total_lines-1) : ((i+1)*per -1);
        args[i].partial_sum = 0;
        args[i].hist_bins = HIST_BINS;
        args[i].hist = calloc(HIST_BINS, sizeof(int));
        args[i].id = i;
        ths[i] = CreateThread(NULL,0,worker,&args[i],0,NULL);
    }

    WaitForMultipleObjects(P, ths, TRUE, INFINITE);
    long long total_sum = 0;
    int *hist = calloc(HIST_BINS, sizeof(int));
    for (int i=0;i<P;i++){
        total_sum += args[i].partial_sum;
        for (int b=0;b<HIST_BINS;b++) hist[b] += args[i].hist[b];
        free(args[i].hist);
    }

    printf("Linhas=%ld Sum=%lld\n", total_lines, total_sum);
    printf("Histograma (bins %d):\n", HIST_BINS);
    for (int b=0;b<HIST_BINS;b++) printf("bin %d: %d\n", b, hist[b]);

    free(hist); free(args); free(ths);
    return 0;
}
