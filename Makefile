# Makefile for trans program
# Cameron Jackson

CC     = gcc -std=c99
CFLAGS = -Wall -Wextra -D_XOPEN_SOURCE=700 -g
LFLAGS = -lrt

.SUFFIXES: .c .o

TARGETS= trans
OBJ    = trans.o

trans: $(OBJ)
	$(CC) -o trans $(OBJ) $(CFLAGS) $(LFLAGS)

trans.o: trans.c

clean:
	rm trans $(OBJ)
