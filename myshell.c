#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <wait.h>
#include <errno.h>
#include <readline/readline.h>
#include <readline/history.h>


#define DEBUG

#define PROMPT "danshell$ "

#define MIN_DIGIT_ASCII_VAL 48
#define MAX_DIGIT_ASCII_VAL 57

#define NUM_CMDS 1

//#define STDIN 0
//#define STDOUT 1

#define EXIT_FORK_ERROR 40
#define EXIT_PIPE_ERROR 41

//extern char** explode_line();

char* rl_gets();
char** create_tokenized( char* string );
void free_tokenized( char** tokenized );
char* find_quote_end( char c, char* cur );
char** array_add_strn( char** arr, char* saddr, char* eaddr, int* size );
char** grow_array( char** arr, int* size );
int execute( char** cmdargs, int in_fd, int* out_fd );
int my_exit( char** line, int pos );
int is_numeric( char* str );

struct node {
  char* str;
  struct node* next;
};
typedef struct node Node;

const int STDOUT = 1;
const int STDIN = 0;

int main( int argc, char** argv) {
  /*
     int i = 0;
     char* line = "";
     char* token;
     Node list = NULL;
     while( line ) {
     line = rl_gets();
     while ( token = next_token( line ) ) {
     printf("[%s] ", token);
     }
     printf("\n");
     }
     return 0;
     */
  int i = 0;
  int size = 2;
  char** array = (char**)malloc( size * sizeof(char**) );
  array[0] = NULL;
  char* w1 = "test";
  char* w2 = "tes\\\"t2";
  char* w3 = "t\\'es t3\\'";
  char* w4 = "'test4'";
  char* w5 = "\"THIS IS A REALLY \\\"LONG\\\" STRING LUL\" AND I DON'T WANT TO SEE THIS SHIT";
  array = array_add_strn( array, w1, w1+strlen(w1), &size );
  printf( "cur size: %d\n", size);
  array = array_add_strn( array, w2, w2+strlen(w2), &size );
  printf( "cur size: %d\n", size);
  array = array_add_strn( array, w3, w3+strlen(w3), &size );
  printf( "cur size: %d\n", size);
  array = array_add_strn( array, w4, w4+strlen(w4), &size );
  printf( "cur size: %d\n", size);
  array = array_add_strn( array, w5, find_quote_end( '"', w5 ), &size );
  printf( "cur size: %d\n", size);

  for( i = 0 ; array[i] != NULL; i++ ) {
    printf("%x, %s\n", array[i], array[i]);
  }


  free_tokenized( array );
  return 0;

}

void free_tokenized( char** arr ) {
  int i = 0;
  while( arr[i] != NULL ) {
    printf(" freeing %x \n", arr[i] );
    free( arr[i] );
    i++;
  }

  printf("FINALLY freeing %x \n", arr );
  free( arr );
}

char** create_tokenized( char* string ) {
  char** tokenarray = (char**)malloc( 5 * sizeof(char**) );
  char* start = string;
  char* end = string;
  int argc = 0;
  int len = 5;
  char c;
  int token_found = 0;

  if( tokenarray != NULL ) {
    tokenarray[0] = NULL;
    while( *end != '\0' ) {
      switch( *end ) {
        case ' ':
        case '\t':
          if( token_found ) {
            argc++;
            tokenarray = array_add_strn( tokenarray, start, end, &len);
            token_found = 0;
            start = end;
          } else {
            end++;
            start++;
          }
          break;
        case '\'':
        case '"':
          if( token_found ) {
            argc++;
            tokenarray = array_add_strn( tokenarray, start, end, &len);
            token_found = 0;
            start = end;
          } else {
            c = *(end+1); /* get next char */
            if( c == '\'' || c == '"' ) { /* if we have a 0 length string */
              argc++;
              tokenarray = array_add_strn( tokenarray, end, end, &len);
              start = end = end+2; /* skip over the double grouping */
            } else {
              c = *end; /* get grouping char " or ' */
              start = ++end; /* move start and end to beginning of string */
              end = find_quote_end( c , end ); /* get the matching end-quote position */
              argc++;
              tokenarray = array_add_strn( tokenarray, start, end, &len);
              start = ++end;
            }
          }
          break;
        default:
          if( token_found ) {
            end++;
          } else {
            token_found = 1;
            start = end;
            end++;
          }
          break;
      } /* end switch */
    } /* end while */

  } /* end if */
  return tokenarray;
}

char* find_quote_end( char c, char* cur ) {
  cur++;
  int found = 0;

  while ( !found ) {
    while( *cur != c && *cur != '\0') cur++;
    if( *cur == c ) {
      if( *(cur-1) == '\\' ) {
        cur++;
      } else {
        found = 1;
      }
    } else {
      found = 1;
    }
  }

  return cur;
}

char** array_add_strn( char** toks, char* saddr, char* eaddr, int* len ) {
  int pos = 0;
  char** array = toks;
  int size = *len;
  int i = 0;
  char c;

  while( *array != NULL ) {
    pos++;
    array++;
  }

  if( pos == (size - 1) ) {
    toks = grow_array( toks , &size );
  }

  array = toks;

  while( *array != NULL ) {
    pos++;
    array++;
  }
  
  printf( "allocating %d for string\n", (eaddr-saddr)+1 );
  *array = malloc( ( (eaddr - saddr) + 1 ) * sizeof(char) );

  while( saddr != eaddr ) {
    switch( *saddr ) {
      case '\\':
        c = *(saddr + 1);
        if( c == '\'' || c == '"' ) {
          (*array)[i] = c;
          i++;
          saddr += 2;
        } else {
          saddr++;
        }
        break;
      case '\'':
      case '"':
        saddr++;
        break;
      default:
        (*array)[i] = *saddr;
        i++;
        saddr++;
        break;
    }
  }

  (*array)[i] = '\0';
  *len = size;
  *(++array) = NULL;

  return toks;
}

char** grow_array( char** arr, int* size ) {
  char** new = (char**)realloc( arr, (*size * 2) * sizeof(char**) );
  *size = *size * 2;
  return new;
}



/**
 * reads a line with the GNU readline library.
 * source: http://cnswww.cns.cwru.edu/php/chet/readline/readline.html
 *
 * @return new string read from user.
 */
char* rl_gets() {
  static char *line_read = (char *)NULL;

  /* If the buffer has already been allocated,
   *      return the memory to the free pool. */
  if (line_read) {
    free (line_read);
    line_read = (char *)NULL;
  }

  /* Get a line from the user. */
  line_read = readline ( PROMPT );

  /* If the line has any text in it,
   *      save it on the history. */
  if (line_read && *line_read)
    add_history (line_read);

  return (line_read);
}


/*
 * Notes: fd[0] = READ end fd[1] = WRITE end
 * dup2( new fd assignment, old fd );
 */

/**
 * Attempts to execute given command
 * *out_fd controls the output behavior. If *out_fd is NULL, execute will
 * return an FD that can be read from to get output. if *out_fd de-references
 * to stdout, then the program will be executed without manipulating where
 * the output is going. If *out_fd de-references to some other number, execute()
 * will send the output to that fd.
 *
 * @param cmdargs a null-terminated string array with the command to exec
 * @param in_fd the file descriptor to read from
 * @param *out_fd where to send output to
 *
 * @return a file descriptor, or 0 for success, or -1 for failure.
 */
int execute( char** cmdargs, int in_fd, int* out_fd ) {
  pid_t pid;
  int i;              
  int fd[2];    /* a file descriptor pipe to send/recieve data on */
  int writefd;  /* the FD we will write to (if not stdout) */
  int ret = 0;

  if( out_fd == NULL ) {  /* We need to return a readable FD to caller */
    if( pipe( fd ) < 0 ) {
      perror( "Fatal pipe error" );
      exit( EXIT_PIPE_ERROR );
    }
    writefd = fd[1];
  } else {
    if( *out_fd != STDOUT ) {
      writefd = *out_fd;
    }
  }

  pid = fork();


  if( pid == 0 ) {
    /*-----------------------------------------------------------------------------
     *  CHILD PROCESS STARTS HERE
     *-----------------------------------------------------------------------------*/

    /* Setup where program is reading from if not stdin */
    if( in_fd != STDIN ) {
      if( dup2( in_fd, STDIN ) < 0 ) { /* make stdin point to in_fd */
        fprintf( 
            stderr, 
            "Unable to make stdin point to %d: %s\n", 
            in_fd,
            strerror( errno ) );
      }

      close( in_fd );
    }

    /* Setup where program is writing output to if not
     * stdout.
     */
    if( out_fd == NULL || *out_fd != STDOUT ) {

      if( out_fd == NULL )
        close( fd[0] ); /* don't need read end of pipe */

      if( dup2( writefd, STDOUT ) < 0 ) { /* make stdout point to writefd */
        fprintf( 
            stderr, 
            "Unable to make stdout point to %d: %s\n", 
            in_fd,
            strerror( errno ) );
      }

      close( writefd );
    }


    if( execvp( cmdargs[0], cmdargs ) < 0 ) {
      fprintf( stderr, "Unable to execute %s: %s\n", cmdargs[0], strerror(errno));
      exit( EXIT_FAILURE );
    }

    /*-----------------------------------------------------------------------------
     *  END CHILD PROCESS
     *-----------------------------------------------------------------------------*/
  } else {
    if( pid < 0 ) {
      perror("Fork fatal error");
      exit( EXIT_FORK_ERROR );
    }
  }

  if( out_fd == NULL ) {
    close( fd[1] );
    ret = fd[0]; /* ret is the fd to read from for prog output */
  }

  if( in_fd != STDIN ) 
    close( in_fd );

  return ret; // return the file descriptor which contains this program's output.
}


/**
 * Exits program. Attempts to use argument supplied.
 * If non-numeric, exits with 0.
 *
 * @param args The string argument following the exit.
 * @return success (will never return)
 */
int my_exit( char** line, int pos ) {
  int exitval = 0;
  if( line[ pos + 1 ] != NULL && is_numeric( line[ pos + 1 ] ) ) {
    exitval = atoi( line[ pos + 1 ]);
  }
  exit( exitval );

  /* never reached */
  return exitval;
}


/**
 * Tests if string is numeric
 * 
 * @param str string
 * @return 0 if false, 1 if true
 */
int is_numeric( char* str ) {
  int ret = 1;
  for( ; *str != '\0'; str++ ) {
    if( *str < MIN_DIGIT_ASCII_VAL || *str > MAX_DIGIT_ASCII_VAL ) {
      ret = 0;
    }
  }

  return ret;
}

