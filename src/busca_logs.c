#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include "busca.h"

#define TAG_BLOCO   1 
#define TAG_TAREFA  2 
#define TAG_NOMES   3
#define TAG_META    4  
#define TAG_NUM     5
#define TAG_TXT     6

#define MAX_CAMINHO 4096

static void morrer(const char *msg) {
    fprintf(stderr, "Erro: %s\n", msg);
    MPI_Abort(MPI_COMM_WORLD, 1);
}


/* resposta do worker */

/* manda o resultado de um arquivo em 3 mensagens */
static void enviar_resultado(long idx, const Resultado *r) {
    long nbytes = 0;
    for (size_t i = 0; i < r->n; i++)
        nbytes += (long)strlen(r->itens[i].texto) + 1;

    long meta[3] = { idx, (long)r->n, nbytes };
    MPI_Send(meta, 3, MPI_LONG, 0, TAG_META, MPI_COMM_WORLD);
    if (r->n == 0) return;

    if (nbytes > INT_MAX) morrer("resultado maior que 2 GB");

    long *num = malloc(r->n * sizeof *num);
    char *txt = malloc((size_t)nbytes);
    if (!num || !txt) morrer("memoria insuficiente no worker");

    char *p = txt;
    for (size_t i = 0; i < r->n; i++) {           /* textos colados, \0 no meio */
        num[i] = r->itens[i].numero;
        size_t t = strlen(r->itens[i].texto) + 1;
        memcpy(p, r->itens[i].texto, t);
        p += t;
    }

    MPI_Send(num, (int)r->n, MPI_LONG, 0, TAG_NUM, MPI_COMM_WORLD);
    MPI_Send(txt, (int)nbytes, MPI_CHAR, 0, TAG_TXT, MPI_COMM_WORLD);

    free(num);
    free(txt);
}

/* recebe de qualquer worker e grava em res[indice].
   devolve o rank: na dinamica, e o worker que ficou livre */
static int receber_resultado(Resultado *res, long n_arq) {
    MPI_Status st;
    long meta[3];
    MPI_Recv(meta, 3, MPI_LONG, MPI_ANY_SOURCE, TAG_META, MPI_COMM_WORLD, &st);

    long idx = meta[0], m = meta[1], nbytes = meta[2];
    if (idx < 0 || idx >= n_arq) morrer("indice invalido na resposta");
    if (m == 0) return st.MPI_SOURCE;

    long *num = malloc((size_t)m * sizeof *num);
    char *txt = malloc((size_t)nbytes);
    if (!num || !txt) morrer("memoria insuficiente no master");

    MPI_Recv(num, (int)m, MPI_LONG, st.MPI_SOURCE, TAG_NUM, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(txt, (int)nbytes, MPI_CHAR, st.MPI_SOURCE, TAG_TXT, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    res[idx].itens = malloc((size_t)m * sizeof *res[idx].itens);
    if (!res[idx].itens) morrer("memoria insuficiente no master");
    res[idx].n = res[idx].cap = (size_t)m;

    const char *p = txt;
    for (long i = 0; i < m; i++) {
        res[idx].itens[i].numero = num[i];
        res[idx].itens[i].texto  = strdup(p);
        if (!res[idx].itens[i].texto) morrer("memoria insuficiente no master");
        p += strlen(p) + 1;
    }

    free(num);
    free(txt);
    return st.MPI_SOURCE;
}

/* busca um arquivo e responde ao master */
static void processar(const char *dir, const char *nome, const char *termo, long idx) {
    char caminho[MAX_CAMINHO];
    int n = snprintf(caminho, sizeof caminho, "%s/%s", dir, nome);

    Resultado r = { NULL, 0, 0 };
    if (n < 0 || (size_t)n >= sizeof caminho)
        fprintf(stderr, "Aviso: caminho longo demais, pulando '%s'.\n", nome);
    else if (buscar_arquivo(caminho, termo, &r) != 0)
        fprintf(stderr, "Aviso: nao consegui ler '%s', pulando.\n", caminho);

    enviar_resultado(idx, &r);
    resultado_liberar(&r);
}

/* saida  */

static void imprimir(char **nomes, long n_arq, Resultado *res, double segundos) {
    long total = 0;

    printf("=== RESULTADOS DA BUSCA ===\n");
    for (long i = 0; i < n_arq; i++) {
        for (size_t k = 0; k < res[i].n; k++)
            printf("%s:%ld: %s\n", nomes[i], res[i].itens[k].numero,
                                   res[i].itens[k].texto);
        total += (long)res[i].n;
    }

    printf("\n=== RESUMO ===\n");
    printf("Total de arquivos: %ld\n", n_arq);
    printf("Total de ocorrencias: %ld\n", total);
    for (long i = 0; i < n_arq; i++)
        if (res[i].n > 0)
            printf("\t%s: %zu ocorrencias\n", nomes[i], res[i].n);
    printf("\nTempo de execucao: %.4f segundos\n", segundos);
}

static void imprimir_carga(const long *carga, int size) {
    long tot = 0, max = 0, min = -1;
    for (int w = 1; w < size; w++) {
        tot += carga[w];
        if (carga[w] > max) max = carga[w];
        if (min < 0 || carga[w] < min) min = carga[w];
    }
    fprintf(stderr, "[stats] arquivos por worker:");
    for (int w = 1; w < size; w++) fprintf(stderr, " w%d=%ld", w, carga[w]);
    fprintf(stderr, "\n[stats] total=%ld min=%ld max=%ld desbalanceamento=%ld\n",
            tot, min, max, max - min);
}

/* ESTATICA*/

/* blocos contiguos. o resto da divisao vai pros
   primeiros workers. divide a QUANTIDADE de arquivos, nao o tamanho deles */
static void master_static(char **nomes, long n_arq, Resultado *res, int size, long *carga) {
    int  nworkers = size - 1;
    long base = n_arq / nworkers, resto = n_arq % nworkers, ini = 0;

    for (int w = 1; w <= nworkers; w++) {
        long nt = base + ((long)(w - 1) < resto ? 1 : 0);

        long nbytes = 0;
        for (long i = ini; i < ini + nt; i++) nbytes += (long)strlen(nomes[i]) + 1;

        long cab[3] = { ini, nt, nbytes };
        MPI_Send(cab, 3, MPI_LONG, w, TAG_BLOCO, MPI_COMM_WORLD);

        if (nt > 0) {
            char *buf = malloc((size_t)nbytes), *p;
            if (!buf) morrer("memoria insuficiente no master");
            p = buf;
            for (long i = ini; i < ini + nt; i++) {
                size_t t = strlen(nomes[i]) + 1;
                memcpy(p, nomes[i], t);
                p += t;
            }
            MPI_Send(buf, (int)nbytes, MPI_CHAR, w, TAG_NOMES, MPI_COMM_WORLD);
            free(buf);
        }
        ini += nt;
    }

    for (long i = 0; i < n_arq; i++)   /* uma resposta por arquivo */
        carga[receber_resultado(res, n_arq)]++;
}

static void worker_static(const char *dir, const char *termo) {
    long cab[3];
    MPI_Recv(cab, 3, MPI_LONG, 0, TAG_BLOCO, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    long ini = cab[0], nt = cab[1], nbytes = cab[2];
    if (nt == 0) return;

    char *buf = malloc((size_t)nbytes);
    if (!buf) morrer("memoria insuficiente no worker");
    MPI_Recv(buf, (int)nbytes, MPI_CHAR, 0, TAG_NOMES, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    const char *nome = buf;
    for (long k = 0; k < nt; k++) {
        processar(dir, nome, termo, ini + k);
        nome += strlen(nome) + 1;
    }
    free(buf);
}

/* DINAMICA */

static void enviar_tarefa(int w, long idx, const char *nome) {
    long nbytes = nome ? (long)strlen(nome) + 1 : 0;
    long t[2] = { idx, nbytes };
    MPI_Send(t, 2, MPI_LONG, w, TAG_TAREFA, MPI_COMM_WORLD);
    if (idx >= 0)
        MPI_Send(nome, (int)nbytes, MPI_CHAR, w, TAG_NOMES, MPI_COMM_WORLD);
}

/* fila no master. cada worker comeca com 1 arquivo; a resposta dele ja serve
   de aviso de que ficou livre. fila vazia -> manda -1 pra encerrar */
static void master_dynamic(char **nomes, long n_arq, Resultado *res, int size, long *carga) {
    long proximo = 0;

    for (int w = 1; w < size; w++) {
        if (proximo < n_arq) enviar_tarefa(w, proximo, nomes[proximo]), proximo++;
        else                 enviar_tarefa(w, -1, NULL);
    }

    for (long recebidos = 0; recebidos < n_arq; recebidos++) {
        int w = receber_resultado(res, n_arq);
        carga[w]++;
        if (proximo < n_arq) enviar_tarefa(w, proximo, nomes[proximo]), proximo++;
        else                 enviar_tarefa(w, -1, NULL);
    }
}

static void worker_dynamic(const char *dir, const char *termo) {
    for (;;) {
        long t[2];
        MPI_Recv(t, 2, MPI_LONG, 0, TAG_TAREFA, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        long idx = t[0], nbytes = t[1];
        if (idx < 0) return;

        char *nome = malloc((size_t)nbytes);
        if (!nome) morrer("memoria insuficiente no worker");
        MPI_Recv(nome, (int)nbytes, MPI_CHAR, 0, TAG_NOMES, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

        processar(dir, nome, termo, idx);
        free(nome);
    }
}


int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);   // quem sou eu: 0 = master, resto = worker
    MPI_Comm_size(MPI_COMM_WORLD, &size);   // quantos processos ao todo

    /* --- argumentos: --dir, --search, --mode, --stats --- */
    const char *dir = NULL, *termo = NULL, *modo = NULL;
    int stats = 0;
    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--stats")  == 0)             stats = 1;
        else if (i + 1 >= argc)                                continue;
        else if (strcmp(argv[i], "--dir")    == 0) dir   = argv[++i];
        else if (strcmp(argv[i], "--search") == 0) termo = argv[++i];
        else if (strcmp(argv[i], "--mode")   == 0) modo  = argv[++i];
    }

    if (!dir || !termo || !modo ||
        (strcmp(modo, "static") != 0 && strcmp(modo, "dynamic") != 0)) {
        if (rank == 0)
            fprintf(stderr,
                "Uso: mpirun -np <n> %s --dir <diretorio> --search <termo> "
                "--mode <static|dynamic> [--stats]\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    if (size < 2) {
        if (rank == 0)
            fprintf(stderr, "Erro: precisa de pelo menos 2 processos (1 master + 1 worker).\n");
        MPI_Finalize();
        return 1;
    }

    int dinamico = (strcmp(modo, "dynamic") == 0);

    if (rank == 0) {
        setvbuf(stdout, NULL, _IOFBF, 1 << 20);   /* a saida pode ter milhares de linhas */

        double t0 = MPI_Wtime();

        char **nomes = NULL;
        long   n_arq = listar_logs(dir, &nomes);
        if (n_arq < 0) morrer("nao consegui ler o diretorio");
        if (n_arq == 0) fprintf(stderr, "Aviso: nenhum arquivo .log em '%s'.\n", dir);

        Resultado *res   = calloc((size_t)(n_arq > 0 ? n_arq : 1), sizeof *res);
        long      *carga = calloc((size_t)size, sizeof *carga);
        if (!res || !carga) morrer("memoria insuficiente no master");

        if (dinamico) master_dynamic(nomes, n_arq, res, size, carga);
        else          master_static (nomes, n_arq, res, size, carga);

        double t1 = MPI_Wtime();   /* mede a busca, nao a impressao */

        imprimir(nomes, n_arq, res, t1 - t0);
        if (stats) imprimir_carga(carga, size);

        for (long i = 0; i < n_arq; i++) resultado_liberar(&res[i]);
        free(carga);
        free(res);
        lista_liberar(nomes, n_arq);
    } else {
        if (dinamico) worker_dynamic(dir, termo);
        else          worker_static (dir, termo);
    }

    MPI_Finalize();
    return 0;
}