# CS 1550 Project 1
# A shell.
# Dan Motles
# Summer 2012
CC=gcc
LD=gcc
CFLAGS=-ggdb -Wall -lfl
LDFLAGS=-ggdb -Wall -lfl

myshell: lex.yy.c myshell.c
	gcc -Wall -ggdb myshell.c lex.yy.c -o myshell -lfl -lreadline -ltermcap
	
crapshell: lex.yy.c crapshell.c
	gcc crapshell.c lex.yy.c -o cshell -lfl

clean:
	rm -f *.o *.exe *.stackdump
