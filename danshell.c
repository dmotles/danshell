/*-----------------------------------------------------------------------------
 *  DANSHELL - a simple linux command shell
 *  Author: Daniel Motles
 *  email: seltom.dan@gmail.com
 *
 *  ALL RIGHTS RESERVERD (C) DAN MOTLES
 *-----------------------------------------------------------------------------*/
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
#include <pwd.h>


//#define DEBUG

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
char* array_remove( char** array, char* item );
char** grow_array( char** arr, int* size );
int is_valid( char** cmdline );
void evaluate( char** cmdline );
int execute( char** cmdargs, int in_fd, int* out_fd );
void my_wait( int pid );
int my_exit( char** line, int pos );
int is_numeric( char* str );

struct node {
  int pid;
  int status;
  struct node* next;
};
typedef struct node Node;

const int STDOUT = 1;
const int STDIN = 0;
char* PROG_NAME;

/**
 * Program execution starts here
 */
int main( int argc, char** argv) {
  int i = 0;
  char* line = "";
  char** tokens;
  PROG_NAME = argv[0];
  while( ( line = rl_gets() ) != NULL ) {
    tokens = create_tokenized( line );
#ifdef DEBUG
    for( i = 0; tokens[i] != NULL; i++ ) {
      printf("[ %s ] ", tokens[i] );
    }
    printf("\n");
#endif
    evaluate( tokens );
    free_tokenized( tokens );
  }
  printf("\n");
  return 0;
}

/**
 * Evaluates a tokenized command line to ensure that the operators are being
 * used properly.
 *
 * @param cmdline the tokenized command line.
 */
int is_valid( char** cmdline ) {
  int i;
  int ret = 1;
  int error = 0;
  char* errorstr;
  /* cmd line validation */
  for( i = 0 ; !error && cmdline[i] != NULL; i++ ) {
    if( i == 0 ) {
      if( strcmp( cmdline[i], "|" ) == 0 ) {
        error = 1;
        errorstr = "Pipes must be preceeded by something.";
      } else if( strcmp( cmdline[i], ">" ) == 0 ) {
        error = 1;
        errorstr = "redirects must be preceeded by something.";
      }
    } else if( cmdline[i+1] == NULL ) {
      if( strcmp( cmdline[i], "|" ) == 0 ) {
        error = 1;
        errorstr = "Pipes must be followed by something.";
      } else if( strcmp( cmdline[i], ">" ) == 0 ) {
        error = 1;
        errorstr = "redirects must be followed by something.";
      }
    }
  }

  if( error ) {
     fprintf( stderr, "%s: %s\n", PROG_NAME, errorstr );
     ret = 0;
  }

  return ret;
}

/**
 * Allows you to add PID's to watch. In order to block and flush all added
 * waits from my_wait's pid queue, call my_wait(0).
 *
 * @param pid - either a process id to add to the wait queue or 0 to block
 *              for all pids and flush the queue
 */
void my_wait( int pid ) {
  static Node* pidlist = NULL;
  Node* newnode;
  Node* ptr = pidlist;
  Node* prev = pidlist;
  if( pid == 0 ) {
    while( ptr != NULL ) {
      prev = ptr;
      waitpid( ptr->pid, &(ptr->status), 0 );
      ptr = ptr->next;
      free(prev);
    }
    pidlist = NULL;
  } else {
    newnode = (Node*)malloc( sizeof(Node) );
    newnode->pid = pid;
    newnode->next = NULL;
    if( pidlist == NULL ) {
      pidlist = newnode;
    } else {
      while( ptr != NULL ) {
        prev = ptr;
        ptr = ptr->next;
      }
      prev->next = newnode;
    } /* end if( pidlist == null ) */
  } /* end if( pid == 0) */

} /* end function */


/**
 * Removes an item from the array list. Will close gaps and
 * free memory associated with the item being freed.
 *
 * @param array the string array
 * @param item the address of the item to be freed
 * @return the item's address if it was removed successfully or NULL for failure.
 */
char* array_remove( char** array, char* item) {
  char** ptr = array;
  char** prev;
  char* ret = item;
  while( *ptr != item && *ptr != NULL ) {
    ptr++;
  }

  if( *ptr != NULL ) {
    free( *ptr );
    while( *ptr != NULL ) {
      prev = ptr;
      ptr++;
      //if( *ptr != NULL ) {
        *prev = *ptr;
      //} else {
      //  *prev = NULL;
      //}
    }
  } else {
    ret = NULL;
  }

  return ret;
}


/**
 * Does the grunt work of evaluating the tokenized command line and setting up
 * an execution path/ opening files/ handling pipes.
 *
 * @param cmdline the tokenized commandline
 */
void evaluate( char** cmdline ) {
  int i,j,k;
  int* out_target = &STDOUT;
  int in_target = STDIN;
  int filefd;
  int file_open = 0;
  char** cmdstring;

  if( !is_valid( cmdline ) ) {
    return;
  }

  /* start parsing commands */
  for( i = 0; cmdline[i] != NULL; i++ ) {
    if( file_open ) {
      in_target = filefd;
      file_open = 0;
    }

    /* go until special operator or NULL */
    for( j = i; cmdline[j] != NULL; j++ ) {
#ifdef DEBUG
      printf( "i:%d j:%d cmdline[j]:%s\n", i, j, cmdline[j]);
#endif

      /* pipe */
      if( strcmp( cmdline[j], "|" ) == 0 ) {
        if( ! file_open ) {
          out_target = NULL;
        }
        break;
      }

      /* redirect */
      if( strcmp( cmdline[j], ">" ) == 0 && ! file_open ) {
        filefd = open(
            cmdline[j+1],
            O_WRONLY | O_CREAT | O_TRUNC,
            S_IRUSR | S_IWUSR | S_IRGRP | S_IRGRP
            );

        if( filefd < 0 ) { /* file open error */
          fprintf( stderr, "%s: opening of %s failed: %s\n",
              PROG_NAME,
              cmdline[j+1],
              strerror( errno ) );
          if( i > 0 ) { /* we need to cleanup for potentially other running programs */
            my_wait( 0 );
            if( in_target != STDIN ) {
              close( in_target );
            }
          }
          return;
        } else { /* file opened fine */
          out_target = &filefd;
          array_remove( cmdline, cmdline[j] );
          array_remove( cmdline, cmdline[j] );
          j--;
          file_open = 1;
        } /* end if( i < 0 ) */
      } /* end redirect op check */

    } /* end inner for loop */

    if( !file_open && cmdline[j] == NULL ) { /* end of line == stdout */
      out_target = &STDOUT;
    }

    cmdstring = (char**)malloc( ((j-i)+1) * sizeof(char**) );

    for( k = 0 ; i < j ; i++, k++ ) {
      cmdstring[k] = cmdline[i];
    }
    cmdstring[k] = NULL;

    in_target = execute( cmdstring, in_target, out_target );
    free( cmdstring );

    if( in_target < 0 ) { /* there was some sort of problem, ABORT MISSION */
      if( i > 0 ) {
        my_wait( 0 ); /* make sure children are DEAD */
      }
      return;
    } /* end if( in_target < 0 ) */


    if( cmdline[i] == NULL ) {
      /* since for loop will increment,
       * don't want to miss end of array */
      i--;
    }

  } /* end outer for loop */

  my_wait( 0 );
}


/**
 * Frees all memory used by the tokenized command line.
 *
 * @param arr the string array to free
 */
void free_tokenized( char** arr ) {
  int i = 0;
  while( arr[i] != NULL ) {
#ifdef DEBUG
    printf(" freeing %x \n", arr[i] );
#endif
    free( arr[i] );
    i++;
  }

#ifdef DEBUG
  printf("FINALLY freeing %x \n", arr );
#endif
  free( arr );
}


/**
 * Parses a single string and tokenizes it based on white space
 * except for enclosing quotes. Returns a string array that must
 * be freed.
 *
 * @param string the command line string to parse
 * @return a new tokenized command line
 */
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


    if( token_found ) {
      tokenarray = array_add_strn( tokenarray, start, end, &len);
    }


  } /* end if */
  return tokenarray;
}

/**
 * Given a string and a quote character, find the closing
 * quote character UNLESS the quote character is preceeded
 * by an escape slash \.
 *
 * @param c the character which is the quote character.
 * @param cur the current position in the string we are searching from
 * @return the matching quote char's position or end of string.
 */
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

/**
 * Add a string to the string array, growing if need be. Since this is
 * being pulled from an existing string without a null terminator, you
 * must specify start and end addresses within the string. You must
 * also pass in a pointer to the length of the array so that it can be
 * grown if it is getting too full.
 *
 * This also will convert escaped quote sequences to just quotes.
 *
 * @param toks the string array
 * @param saddr string's starting address
 * @param eaddr string's ending address
 * @param len address to array length variable.
 */
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

#ifdef DEBUG
  printf( "allocating %d for string\n", (eaddr-saddr)+1 );
#endif
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


/**
 * Doubles the size of an array using realloc. Must pass in size to get new size.
 *
 * @param arr array address
 * @param size the address to the size var
 * @return the address of the re-sized array. It may have changed with realloc.
 */
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
  int fd[2];    /* a file descriptor pipe to send/recieve data on */
  int writefd;  /* the FD we will write to (if not stdout) */
  int ret = 0;
  char* cd_target;
  struct passwd* pw;

  if( strcmp( cmdargs[0] , "exit" ) == 0 ) {
    my_exit( cmdargs, 0);
  } else if( strcmp( cmdargs[0] , "cd" ) == 0 ) {
    if ( cmdargs[1] == NULL ) {
      pw = getpwuid( getuid() );
      cd_target = pw->pw_dir;
    } else {
      cd_target = cmdargs[1];
    }
    if( ( ret = chdir( cmdargs[1] ) ) < 0 ) {
        fprintf(
            stderr,
            "%s: unable to cd to %s: %s\n",
            PROG_NAME,
            cd_target,
            strerror( errno )
            );
    } else {
      printf("cwd: %s\n", cd_target);
    }
    return 0;
  }

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
            "%s: Unable to make stdin point to %d: %s\n",
            PROG_NAME,
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
            "%s: Unable to make stdout point to %d: %s\n",
            PROG_NAME,
            in_fd,
            strerror( errno )
            );
      }

      close( writefd );
    }


    if( execvp( cmdargs[0], cmdargs ) < 0 ) {

      fprintf(
          stderr,
          "%s: Unable to execute %s: %s\n",
          PROG_NAME,
          cmdargs[0],
          strerror(errno)
          );

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

    my_wait( pid );
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

