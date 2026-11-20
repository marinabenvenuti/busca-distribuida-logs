# notas do t2

## estrutura

- src/busca.c|h -> nucleo: lista o diretorio e busca dentro de um arquivo. nao sabe nada de mpi
- src/busca_logs.c -> programa mpi (master/worker), as duas estrategias
- as duas estrategias usam o mesmo busca.c, por isso tem que dar o mesmo resultado

## como gerar os conjuntos de teste

o gerador do professor so faz arquivos do mesmo tamanho. pro cenario de
"tamanhos diferentes" que o enunciado pede, chamar ele 1x por arquivo e
renomear. os 3 conjuntos abaixo tem 24 arquivos e 192.000 linhas cada:

    # homogeneo: 24 iguais
    ./gerador_logs_t2 24 8000 8 42 logs_homog

    # heterogeneo: trocar a lista de linhas conforme o caso
    #   espalhado  -> 1600 1600 40000 1600 ... (grandes no meio)
    #   concentrado-> 40000 40000 40000 40000 1600 1600 ... (grandes no inicio)
    i=1; mkdir -p logs_het
    for n in 40000 40000 40000 40000 1600 1600 1600 1600 1600 1600 1600 1600 \
             1600 1600 1600 1600 1600 1600 1600 1600 1600 1600 1600 1600; do
      ./gerador_logs_t2 1 $n 8 $((100+i)) tmp$i > /dev/null
      mv tmp$i/log_0001.log logs_het/$(printf 'log_%04d.log' $i); rmdir tmp$i
      i=$((i+1))
    done

## decisoes da busca

- so pega arquivo .log. ignora o que comeca com ponto e usa stat pra confirmar que e arquivo normal
- strstr -> busca exata e sensivel a maiusculas. testa uma vez por linha, entao linha com o termo 2x conta 1x
- "total de arquivos" = arquivos lidos. no resumo so aparecem os que tiveram pelo menos uma ocorrencia

## decisoes do mpi

- todos os ranks leem o mesmo argv, entao --dir e --search ja estao em todo mundo. so a lista de arquivos precisa ser enviada
- so o master faz readdir
- o worker recebe os NOMES e abre o arquivo do disco
- a resposta vai em 3 mensagens por arquivo: meta{indice, qtd linhas, bytes}, vetor de numeros, textos colados com \0 no meio
- o indice global viaja junto com a resposta -> o master monta a saida em ordem mesmo recebendo fora de ordem
- master recebe com MPI_ANY_SOURCE e espera exatamente n_arquivos respostas
- worker manda resposta mesmo com 0 ocorrencias, senao a contagem do master nao fecha
- estatica: blocos contiguos, igual a figura. o resto vai pros primeiros workers, entao a diferenca nunca passa de 1 arquivo (em QUANTIDADE, nao em tamanho)
- dinamica: a propria resposta do worker e o aviso de "estou livre". nao precisa de mensagem separada de pedido
- dinamica: encerramento = tarefa com indice -1. cada worker recebe exatamente um
- worker que nao pegou tarefa nenhuma recebe o -1 ja na distribuicao inicial

## medição

## duvidas

-
