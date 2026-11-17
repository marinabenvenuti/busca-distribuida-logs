#ifndef BUSCA_H
#define BUSCA_H

#include <stddef.h>

/* Nucleo de busca, sem MPI. Usado pelas duas estrategias de distribuicao. */

typedef struct {
    long  numero;   /* numero da linha, comecando em 1 */
    char *texto;    /* conteudo da linha, sem o '\n' */
} Ocorrencia;

typedef struct {
    Ocorrencia *itens;
    size_t      n;
    size_t      cap;
} Resultado;

/* Arquivos .log do diretorio, em ordem alfabetica. Devolve quantos achou ou -1. */
long listar_logs(const char *dir, char ***nomes);
void lista_liberar(char **nomes, long n);

/* Busca exata e sensivel a maiusculas; linha com varias ocorrencias conta uma.
   Devolve 0 ou -1. Nos dois casos *r deve ser liberado. */
int  buscar_arquivo(const char *caminho, const char *termo, Resultado *r);
void resultado_liberar(Resultado *r);

#endif