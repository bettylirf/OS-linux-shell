CC=gcc
CFLAGS=-g -pedantic -std=gnu17 -Wall -Werror -Wextra

.PHONY: all
all: shell

shell: shell.o

shell.o: shell.c

.PHONY: clean
clean: rm -f *.o shell