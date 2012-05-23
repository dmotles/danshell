#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>

#define TRUE 1

extern char** explode_line();

int main( int argc, char** argv) {
  int i = 0;
  char** linetoks = (char**)-1; /* SET TO -1 (0xFFFF) TO AVOID BEING NULL */ 

  while( TRUE ) {
    linetoks = explode_line();
    for(i = 0; linetoks[i] != NULL; i++) {
      printf("Argument %d: %s\n", i, linetoks[i]);
    }
  }

  return 0;
}


void exit()
