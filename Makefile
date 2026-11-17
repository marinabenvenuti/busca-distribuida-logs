CC      = gcc
MPICC   = mpicc
CFLAGS  = -O2 -Wall -Wextra

all: busca_logs gerador_logs_t2

busca_logs: src/busca_logs.c src/busca.c src/busca.h
	$(MPICC) $(CFLAGS) -o $@ src/busca_logs.c src/busca.c

gerador_logs_t2: src/gerador_logs_t2.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f busca_logs gerador_logs_t2

.PHONY: all clean