# T2 - Busca distribuida em arquivos de log

INE5645 - Programacao Paralela e Distribuida - UFSC - 2026/2

Busca um termo em um conjunto de arquivos de log, distribuindo os arquivos
entre processos MPI no padrao master-worker. Implementa duas estrategias de
distribuicao: estatica e dinamica.

## Dependencias

- `gcc`
- uma implementacao de MPI (Open MPI ou MPICH)

## Compilacao

    make

Gera dois executaveis: `busca_logs` (aplicacao MPI) e `gerador_logs_t2`
(gerador de conjuntos de entrada, fornecido pelo professor).

## Execucao

Gerar um conjunto de logs (100 arquivos, 10000 linhas cada, 8 palavras por
linha, semente 42, no diretorio `logs/`):

    ./gerador_logs_t2 100 10000 8 42 logs

Rodar a busca:

    mpirun -np 5 ./busca_logs --dir logs --search ERROR --mode dynamic

## Estrutura

    src/busca.c, src/busca.h   nucleo da busca, sem MPI
    src/busca_logs.c           aplicacao MPI (master/workers)
    src/gerador_logs_t2.c      gerador de entradas (do professor)
    docs/                      enunciado e notas de desenvolvimento

## Integrantes (Grupo T2 05328A C)

- Cecilia Zica Camargo (24208346)
- Joao Paulo Decker Oleinik (24202528)
- Marina Benvenuti Cardeal (23103131)
