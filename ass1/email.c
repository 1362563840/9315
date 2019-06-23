/*
 * src/tutorial/email.c
 *
 ******************************************************************************
  This file contains routines that can be bound to a Postgres backend and
  called by the backend in the process of processing queries.  The calling
  format for these routines is dictated by Postgres architecture.
******************************************************************************/

#include "postgres.h"

#include "fmgr.h"
#include "libpq/pqformat.h"		/* needed for send/recv functions */

#include <ctype.h>
#include <regex.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define MAXWORDLENGTH  256 + 1
#define MAXWORD  ( 256 / 2 ) + 1
#define LOCAL_CHEKC_CODE  1
#define DOMAIN_CHEKC_CODE  2
bool CheckEmail(char **email, char **_local, char **_domain);
char **CheckParts(char *parts, int *size, int which_part);
void destroy2D(char **target, int size) ;
char *ExtracWord(char *parts, int begin, int end);

PG_MODULE_MAGIC;

typedef struct EmailAddr
{
    char *local;
    char *domain;
} EmailAddr;

char *ExtracWord(char *parts, int begin, int end) {
  // + 1 is because if begin is 0, end is 3, then total length is actually 4
  // but 3 - 0 = 3, so need + 1 
  // another is for '\0'
  char *result = calloc( ( end - begin + 1 + 1 ), sizeof(char)  );
  if( result == NULL ) {
     ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("not enough for calloc -> result\n")
                 )
                );
      exit(1);
  }
  int start = 0;
  int i = begin;
  for( i = begin ; i <= end ; i++ ) {
    result[start] = parts[i];
    start++;
  }
  return result;
}

void destroy2D(char **target, int size) {
  int i = 0;
  for( i = 0 ; i < size ; i++ ) {
      char *temp = target[ i ];
      free( temp );
  }
  free(target);
}

/**
 * for "which_part", if it is 1, then it is local, otherwise 2
 */
char **CheckParts(char *parts, int *size, int which_part) {

  if( isalpha( parts[0] ) == 0  ) {
    ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("start of words is not alpha %s\n", parts)
                )
                );
    exit(1);
  }
  unsigned int length = strlen(parts);
  /**
   * need to consider if last bit is '\0'`
   * check
   */
  if( parts[length - 1 ] == '\0' ) {
    ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("should never happen %s\n", parts)
                 )
                );
    exit(1);
    length = length - 1;
  }
  if( isalpha( parts[ length - 1 ] ) == 0 && isdigit( ( parts[ length - 1 ] ) ) == 0 ) {
    ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("last letter is not correct, should either alphet or digit, but %c\n",parts[ length - 1 ])
                 )
                );
    exit(1);
  }
  // create 2d char which is 1d string
  char **words = malloc( sizeof( char * ) * ( MAXWORD ) );
  if( words == NULL ) {
      ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("not enough for malloc -> words\n")
                 )
                );
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
  int i = 0;
  for( i = 0 ; i < length ; i++ ) {
      // check if this character is leter or digit or hyphens('-')
      if( isalpha( parts[i] ) == 0 && isdigit( ( parts[i] ) ) == 0 && parts[i] != '-' && parts[i] != '.' ) {
          ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("This character is not alphet or digit or hyphens >>%c\n", parts[i])
                 )
                );
          exit(1);
      }
      if( parts[i] == delim ) {
      // check if meets two consecutive delimeter
        if( last_end != -1 && last_end + 1 == i - 1 ) {
          ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("invalid email parts, two consecutive dot\n")
                 )
                );
          exit(1);
        }else {
          end = i - 1;
          last_end = end;
          int temp_length = end - start + 1 + 1;
          char *temp = ExtracWord(parts, start, end);
          words[how_many] = temp;
          // create "end of string"
          words[how_many][ temp_length - 1 ] = '\0';
          how_many++;
        }
        // if next one is still delimeter, this for loop should just end and program stops
        start = i + 1;
    }
  }
  // extract last word
  end = length - 1;
  char *temp = ExtracWord(parts, start, end);
  words[how_many] = temp;
  int temp_length = end - start + 1 + 1;
  words[how_many][ temp_length - 1 ] = '\0';
  how_many++;

  if( which_part == 1 ) {
    if( how_many < 1 ) {
      ereport(ERROR,
            (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
            errmsg("local part needs at least one part, but currently %d parts\n", how_many)
              )
            );
      exit(1);
    }
  }
  if( which_part == 2  ) {
    if( how_many < 2 ) {
      ereport(ERROR,
            (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
            errmsg("domain part needs at least two parts, but currently %d parts\n", how_many)
              )
            );
      exit(1);
    }
  }

  *size = how_many;
  return words;
}

/**
 * parse string by reference
 * do not modified varaible "email" since it is read-only
 */
bool CheckEmail(char **email, char **_local, char **_domain){

    regex_t regex;
    int reti;
    char msgbuf[100]={};
    // char original[257];
    char local[ MAXWORDLENGTH ]={};
    char domain[ MAXWORDLENGTH ]={};

    /**
     * for local and domain
     */
    size_t maxGroups = 3;
    regmatch_t groupArray[maxGroups];

    /* Compile regular expression */
    /**
     * an email address has two parts, Local and Domain, separated by an '@' char
     */
    reti = regcomp(&regex, "^([^@]+)@([^@]+)$", REG_EXTENDED);
    if ( reti != 0 ) {
        ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("can not create regular expression: ^([^@]+)@([^@]+)$\n")
                 )
                );
        exit(1);
    }

    /* Execute regular expression */
    reti = regexec(&regex, email[0], maxGroups, groupArray, 0);
    if ( reti == 0 ) {
        // puts("Match\n");
    }
    else if (reti == REG_NOMATCH) {
        ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid email adddress, \"%s\" ", email[0])
                 )
                );
    }
    else {
        regerror(reti, &regex, msgbuf, sizeof(msgbuf));
        ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("special error\n, \"%s\" ", email[0])
                 )
                );
        exit(1);
    }

    // extract local and domain
    /**
     * the structure of regmatch_t is
     * {
     *  regoff_t 	rm_so // treat them as integer
     *  regoff_t 	rm_eo // integer
     * }
     * the first one is the start of a given string, in this case "email[0]" including
     * the latter one is the end of given string, excluding
     */

    // make a copy of orgin -> "email[0]" since it needs modified
    char *temp_copy_email = email[0];
    // let the end of char[?] to be "End of string"
    temp_copy_email[ groupArray[1].rm_eo ] = '\0';
    // move to start position, it should be zero
    temp_copy_email = temp_copy_email + groupArray[1].rm_so;
    strcpy( &local[0], temp_copy_email );


    temp_copy_email = email[0];

    temp_copy_email[ groupArray[2].rm_eo ] = '\0';
    // move to start position, it not be zero
    temp_copy_email = temp_copy_email + groupArray[2].rm_so;
    strcpy( &domain[0], temp_copy_email );

    int local_size = 0;
    int domain_size = 0;
    char **local_parts = CheckParts( local, &local_size, LOCAL_CHEKC_CODE );
    char **domain_parts = CheckParts( domain, &domain_size, DOMAIN_CHEKC_CODE );
    destroy2D(local_parts, local_size);
    destroy2D(domain_parts, domain_size);
    strcpy(_local[0], &local[0]);
    strcpy(_domain[0], &domain[0]);
    return true;
}

/*****************************************************************************
 * Input/Output functions
 *****************************************************************************/

PG_FUNCTION_INFO_V1(email_in);

Datum
email_in(PG_FUNCTION_ARGS)
{
	char        *str = PG_GETARG_CSTRING(0);
	char	    *local = (char *)calloc( ( MAXWORDLENGTH ), sizeof(char) );
  char	    *domain = (char *)calloc( ( MAXWORDLENGTH ), sizeof(char) );
  // a series chars input, check whether it satisfy the rules
  CheckEmail( &str, &local, &domain );

  /**
   * after separate string, right now, 
   * we can know the true length of input string by using strlen() with '\0'
   * 
   * use palloc, i do not know why, but just use palloc
   * 
   * strlen() will not count '\0', so you need to + 1 
   */
  EmailAddr    *result = (EmailAddr *) palloc( sizeof( EmailAddr ) );
  result -> local = (char *)calloc( strlen(local) + 1 , 1 );
  result -> domain = (char *)calloc( strlen(domain) + 1, 1 );
  /**
   * shoud just be perfect exactly same space incluing '\0'
   */
  int i = 0;
  for( i = 0 ; i < strlen(local) + 1 ; i++ ) {
    local[i] = tolower(local[i]);
  }
  i = 0;
  for( i = 0 ; i < strlen(local) + 1 ; i++ ) {
    domain[i] = tolower(domain[i]);
  }
  /**
   * the palloc block is tricky
   */
  i = 0;
  for( i = 0 ; i < strlen(local) + 1 ; i++ ) {
    result -> local[ i ] = local[ i ];
  }
  // debug -----------------------------------------------
  if( result -> local[ strlen(local) + 1 - 1 ] != '\0' ) {
    ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("origin local is %s, current local is %s\n", local, result -> local)
                 )
                );
        exit(1);
  }
  // debug -----------------------------------------------
  i = 0;
  for( i = 0 ; i < strlen(domain) + 1 ; i++ ) {
    result -> domain[ i ] = domain[ i ];
  }
  // memcpy(result -> local, local, strlen(local) + 1);
  // memcpy(result -> domain, domain, strlen(domain) + 1);

  // ereport(ERROR,
	// 			(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
	// 			 errmsg("origin locai is %s, local is %s and size is %d\n", local, result -> local, strlen(local) + 1)
  //                )
  //               );
  // exit(1);

  free(local);
  free(domain);
	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(email_out);

Datum
email_out(PG_FUNCTION_ARGS)
{
	EmailAddr    *email = (EmailAddr *) PG_GETARG_POINTER(0);
	char	   *result;

	result = psprintf("%s@%s", email->local, email->domain);
	PG_RETURN_CSTRING(result);
}


/*****************************************************************************
 * New Operators
 *
 * A practical Complex datatype would provide much more than this, of course.
 *****************************************************************************/



// /*****************************************************************************
//  * Operator class for defining B-tree index
//  *
//  * It's essential that the comparison operators and support function for a
//  * B-tree index opclass always agree on the relative ordering of any two
//  * data values.  Experience has shown that it's depressingly easy to write
//  * unintentionally inconsistent functions.  One way to reduce the odds of
//  * making a mistake is to make all the functions simple wrappers around
//  * an internal three-way-comparison function, as we do here.
//  *****************************************************************************/


static int
email_abs_cmp_internal(EmailAddr * a, EmailAddr * b)
{
  // diff in domain part
  if( strcmp( a -> domain, b -> domain ) < 0 ) {
    return  -1;
  }
  if( strcmp( a -> domain, b -> domain ) > 0 ) {
    return  1;
  }

   // diff in local part
	if( strcmp( a -> local, b -> local ) < 0 ) {
    return  -1;
  }
  if( strcmp( a -> local, b -> local ) > 0 ) {
    return 1;
  }
  return 0;
}

static int
email_domain_cmp_internal(EmailAddr * a, EmailAddr * b)
{
  // diff in domain part
  if( strcmp( a -> domain, b -> domain ) < 0 ) {
    return  -1;
  }
  if( strcmp( a -> domain, b -> domain ) > 0 ) {
    return  1;
  }
  return 0;
}


PG_FUNCTION_INFO_V1(email_abs_lt);

Datum
email_abs_lt(PG_FUNCTION_ARGS)
{
	EmailAddr    *a = (EmailAddr *) PG_GETARG_POINTER(0);
	EmailAddr    *b = (EmailAddr *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_abs_cmp_internal(a, b) < 0);
}

PG_FUNCTION_INFO_V1(email_abs_le);

Datum
email_abs_le(PG_FUNCTION_ARGS)
{
	EmailAddr    *a = (EmailAddr *) PG_GETARG_POINTER(0);
	EmailAddr    *b = (EmailAddr *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_abs_cmp_internal(a, b) <= 0);
}

PG_FUNCTION_INFO_V1(email_abs_eq);

Datum
email_abs_eq(PG_FUNCTION_ARGS)
{
	EmailAddr    *a = (EmailAddr *) PG_GETARG_POINTER(0);
	EmailAddr    *b = (EmailAddr *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_abs_cmp_internal(a, b) == 0);
}

PG_FUNCTION_INFO_V1(email_abs_inequality);

Datum
email_abs_inequality(PG_FUNCTION_ARGS)
{
	EmailAddr    *a = (EmailAddr *) PG_GETARG_POINTER(0);
	EmailAddr    *b = (EmailAddr *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_abs_cmp_internal(a, b) != 0);
}

PG_FUNCTION_INFO_V1(email_abs_ge);

Datum
email_abs_ge(PG_FUNCTION_ARGS)
{
	EmailAddr    *a = (EmailAddr *) PG_GETARG_POINTER(0);
	EmailAddr    *b = (EmailAddr *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_abs_cmp_internal(a, b) >= 0);
}

PG_FUNCTION_INFO_V1(email_abs_gt);

Datum
email_abs_gt(PG_FUNCTION_ARGS)
{
	EmailAddr    *a = (EmailAddr *) PG_GETARG_POINTER(0);
	EmailAddr    *b = (EmailAddr *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_abs_cmp_internal(a, b) > 0);
}

PG_FUNCTION_INFO_V1(email_abs_cmp);

Datum
email_abs_cmp(PG_FUNCTION_ARGS)
{
	EmailAddr    *a = (EmailAddr *) PG_GETARG_POINTER(0);
	EmailAddr    *b = (EmailAddr *) PG_GETARG_POINTER(1);

	PG_RETURN_INT32(email_abs_cmp_internal(a, b));
}

PG_FUNCTION_INFO_V1(email_abs_tilde);

Datum
email_abs_tilde(PG_FUNCTION_ARGS)
{
	EmailAddr    *a = (EmailAddr *) PG_GETARG_POINTER(0);
	EmailAddr    *b = (EmailAddr *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_domain_cmp_internal(a, b) == 0);
}

PG_FUNCTION_INFO_V1(email_abs_tilde_neg);

Datum
email_abs_tilde_neg(PG_FUNCTION_ARGS)
{
	EmailAddr    *a = (EmailAddr *) PG_GETARG_POINTER(0);
	EmailAddr    *b = (EmailAddr *) PG_GETARG_POINTER(1);

	PG_RETURN_BOOL(email_domain_cmp_internal(a, b) != 0);
}