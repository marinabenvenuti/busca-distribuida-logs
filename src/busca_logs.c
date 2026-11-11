#include <stdio.h>
#include <string.h>
#include <mpi.h>

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
        printf("master: dir=%s termo=%s modo=%s workers=%d\n", dir, termo, modo, size - 1);
    } else {
        printf("worker %d: pronto\n", rank);
    }

    MPI_Finalize();
    return 0;
}