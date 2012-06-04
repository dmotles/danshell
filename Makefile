# CS 1550 Project 1
# A shell.
# Dan Motles
# Summer 2012
CC=gcc
LD=gcc
CFLAGS=-ggdb -Wall -lfl
LDFLAGS=-ggdb -Wall -lfl

danshell: danshell.c
	gcc -Wall -ggdb danshell.c -o danshell -lreadline -ltermcap
	
clean:
	rm -f *.o *.exe *.stackdump
