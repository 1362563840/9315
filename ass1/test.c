#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAXWORDLENGTH  128 + 1
#define MAXWORD  ( 128 / 2 ) + 1

char *ExtracWord(char *parts, int begin, int end) {
  // + 1 is for '\0'
  char *result = calloc( ( end - begin + 1 ), sizeof(char)  );
  if( result == null ) {
      // print out
      exit(1);
  }
  int start = 0;
  for( int i = begin ; i <= end ; i++ ) {
    result[start] = parts[i];
    start++;
  }
  return result;
}

void destroy2D(char **target, int size) {
    for( int i = 0 ; i < size ; i++ ) {
        char *temp = target[ i ];
        free( temp );
    }
    free(target);
}

char **CheckParts(char *parts, int *size)  {
  if( isalpha( parts[0] ) == 0  ) {
    printf("invalid email\n");
    exit(1);
  }
  unsigned int length = strlen(parts);
  /**
   * need to consider if last bit is '\0'`
   * check
   */
  if( parts[ strlen(parts) - 1 ] == '\0' ) {
      length = length - 1;
  }
  if( isalpha( parts[ length - 1 ] ) == 0 && isdigit( ( parts[ length - 1 ] - '0' ) == 0 ) ) {
    printf("invalid email\n");
    exit(1);
  }
  // create 2d char which is 1d string
  char **words = malloc( sizeof( char * ) * ( MAXWORD ) );
  if( words == null ) {
      // print out
      exit(1);
  }
  // char words[ MAXWORD ] [ MAXWORDLENGTH ];


  int how_many = 0;
  char delim = '.';
  // both including
  int last_end = -1;
  int start = 0;
  int end = 0;
  /*  01234567
   *  abc..abc
   *  for exmaple, when i == 4 and it is delimeter
   *  last_end shoud be 2,
   *  then check if is the second consecutive delimeter
  */
  for( int i = 0 ; i < length ; i++ ) {
      if( parts[i] == delim ) {
      // check if meets two consecutive delimeter
        if( last_end != -1 && last_end + 1 == i - 1 ) {
          printf("error\n");
          exit(1);
        }else {
          end = i - 1;
          last_end = end;
          int temp_length = end - start + 1;
          char *temp = ExtracWord(parts, start, end);
          words[how_many] = temp;
          // create "end of string"
          words[how_many][temp_length] = '\0';
          how_many++;
        }
        // if next one is still delimeter, this for loop should just end and program stops
        start = i + 1;
    }
  }
  // extract last word
  end = length;
  char *temp = ExtracWord(parts, start, end);
  words[how_many] = temp;
  int temp_length = end - start + 1;
  words[how_many][temp_length] = '\0';
  how_many++;


  *size = how_many;
  return words;
}


int main()
{
    int size = 0;
    char *parts = "athos.fuck.liu..";
    char **result = CheckParts(parts, &size);
    for( int i = 0 ; i < size ; i++ ) {
        printf(">>>%s\n", result[i]);
    }
    destroyArray(result);
}
