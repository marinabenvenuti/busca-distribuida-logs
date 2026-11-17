#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "busca.h"

static int termina_em_log(const char *nome) {
    size_t n = strlen(nome);
    return n > 4 && strcmp(nome + n - 4, ".log") == 0;
}

static int cmp_nome(const void *a, const void *b) {
    return strcmp(*(const char *const *)a, *(const char *const *)b);
}

long listar_logs(const char *dir, char ***nomes) {
    DIR *d = opendir(dir);
    if (!d) return -1;

    size_t cap = 64, n = 0;
    char **v = malloc(cap * sizeof *v);
    if (!v) { closedir(d); return -1; }

    struct dirent *e;
    while ((e = readdir(d)) != NULL) {
        if (e->d_name[0] == '.') continue;    
        if (!termina_em_log(e->d_name)) continue; /* so os .log do gerador */

        char caminho[4096];
        int k = snprintf(caminho, sizeof caminho, "%s/%s", dir, e->d_name);
        if (k < 0 || (size_t)k >= sizeof caminho) continue;

        struct stat st;
        if (stat(caminho, &st) != 0 || !S_ISREG(st.st_mode)) continue;

        if (n == cap) {
            char **t = realloc(v, cap * 2 * sizeof *v);
            if (!t) { lista_liberar(v, (long)n); closedir(d); return -1; }
            v = t; cap *= 2;
        }
        if ((v[n] = strdup(e->d_name)) == NULL) {
            lista_liberar(v, (long)n); closedir(d); return -1;
        }
        n++;
    }
    closedir(d);

    qsort(v, n, sizeof *v, cmp_nome);

    *nomes = v;
    return (long)n;
}

void lista_liberar(char **nomes, long n) {
    if (!nomes) return;
    for (long i = 0; i < n; i++) free(nomes[i]);
    free(nomes);
}



static int guardar(Resultado *r, long numero, const char *texto) {
    if (r->n == r->cap) {
        size_t nova = r->cap ? r->cap * 2 : 16;
        Ocorrencia *t = realloc(r->itens, nova * sizeof *t);
        if (!t) return -1;
        r->itens = t; r->cap = nova;
    }
    char *copia = strdup(texto);
    if (!copia) return -1;

    r->itens[r->n].numero = numero;
    r->itens[r->n].texto  = copia;
    r->n++;
    return 0;
}

int buscar_arquivo(const char *caminho, const char *termo, Resultado *r) {
    r->itens = NULL; r->n = 0; r->cap = 0;

    FILE *f = fopen(caminho, "r");
    if (!f) return -1;

    char   *linha = NULL;
    size_t  cap_linha = 0;
    ssize_t lidos;
    long    numero = 0;
    int     falhou = 0;

    /*cresce o buffer sozinho: nao ha limite de tamanho de linha */
    while ((lidos = getline(&linha, &cap_linha, f)) != -1) {
        numero++;

        /* tira o '\n' e tambem o '\r'  */
        while (lidos > 0 && (linha[lidos - 1] == '\n' || linha[lidos - 1] == '\r'))
            linha[--lidos] = '\0';


        if (strstr(linha, termo) != NULL && guardar(r, numero, linha) != 0) {
            falhou = 1;
            break;
        }
    }

    free(linha);
    fclose(f);

    if (falhou) { resultado_liberar(r); return -1; }
    return 0;
}

void resultado_liberar(Resultado *r) {
    if (!r) return;
    for (size_t i = 0; i < r->n; i++) free(r->itens[i].texto);
    free(r->itens);
    r->itens = NULL; r->n = 0; r->cap = 0;
}


