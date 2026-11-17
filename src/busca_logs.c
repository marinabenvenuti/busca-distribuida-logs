#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>

#include "busca.h"

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);   // quem sou eu: 0 = master, resto = worker
    MPI_Comm_size(MPI_COMM_WORLD, &size);   // quantos processos ao todo

    /* --- argumentos: --dir, --search, --mode --- */
    const char *dir = NULL, *termo = NULL, *modo = NULL;
    for (int i = 1; i < argc - 1; i++) {
        if      (strcmp(argv[i], "--dir")    == 0) dir   = argv[++i];
        else if (strcmp(argv[i], "--search") == 0) termo = argv[++i];
        else if (strcmp(argv[i], "--mode")   == 0) modo  = argv[++i];
    }

    if (!dir || !termo || !modo ||
        (strcmp(modo, "static") != 0 && strcmp(modo, "dynamic") != 0)) {
        if (rank == 0)
            fprintf(stderr,
                "Uso: mpirun -np <n> %s --dir <diretorio> --search <termo> "
                "--mode <static|dynamic>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }

    if (size < 2) {
        if (rank == 0)
            fprintf(stderr, "Erro: precisa de pelo menos 2 processos (1 master + 1 worker).\n");
        MPI_Finalize();
        return 1;
    }

    if (rank == 0) {

        setvbuf(stdout, NULL, _IOFBF, 1 << 20);

        double t0 = MPI_Wtime();

        char **nomes = NULL;
        long   n_arq = listar_logs(dir, &nomes);
        if (n_arq < 0) {
            fprintf(stderr, "Erro: nao consegui ler o diretorio '%s'.\n", dir);
            MPI_Abort(MPI_COMM_WORLD, 1);
        }
        if (n_arq == 0)
            fprintf(stderr, "Aviso: nenhum arquivo .log em '%s'.\n", dir);

        Resultado *res = calloc((size_t)(n_arq > 0 ? n_arq : 1), sizeof *res);
        if (!res) {
            fprintf(stderr, "Erro: memoria insuficiente.\n");
            MPI_Abort(MPI_COMM_WORLD, 1);
        }

        for (long i = 0; i < n_arq; i++) {
            char caminho[4096];
            int k = snprintf(caminho, sizeof caminho, "%s/%s", dir, nomes[i]);
            if (k < 0 || (size_t)k >= sizeof caminho) {
                fprintf(stderr, "Aviso: caminho longo demais, pulando '%s'.\n", nomes[i]);
                continue;
            }
            if (buscar_arquivo(caminho, termo, &res[i]) != 0)
                fprintf(stderr, "Aviso: nao consegui ler '%s', pulando.\n", caminho);
        }

        double t1 = MPI_Wtime();   /* o tempo medido e o da busca, nao o da impressao */

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
        printf("\nTempo de execucao: %.4f segundos\n", t1 - t0);

        for (long i = 0; i < n_arq; i++) resultado_liberar(&res[i]);
        free(res);
        lista_liberar(nomes, n_arq);
    }

    MPI_Finalize();
    return 0;
}