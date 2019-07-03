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

#include "access/hash.h"

#define MAXWORDLENGTH  256 + 1
#define MAXWORD  ( 256 / 2 ) + 1
#define LOCAL_CHEKC_CODE  1
#define DOMAIN_CHEKC_CODE  2
bool CheckEmail(char **email, char **_local, char **_domain);
void CheckParts(char *parts, int *size, int which_part);
// char *ExtracWord(char *parts, int begin, int end);

PG_MODULE_MAGIC;

/**
 * first end includes the '\0' for local
 * second end includes the '\0' for domain
 */
typedef struct EmailAddr
{
    char vl_len_[4];
    int first_end;
    int second_end;
    char all[];
} EmailAddr; 

/**
 * for "which_part", if it is 1, then it is local, otherwise 2
 */
void CheckParts(char *parts, int *size, int which_part) {

  if( isalpha( parts[0] ) == 0  ) {
    ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("start of words(\"%c\") is not alpha \"%s\"\n", parts[0], parts)
                )
                );
    exit(1);
  }
  unsigned int length = strlen(parts);
  /**
   * check  check  check  check  check  check  check  
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
        }
        /** 
         * check the one before the delimeter
         * because we already make sure start character is not dot,
         * so i - 1 should be within variable "length"
        */
        if( parts[ i - 1 ] == '-' ) {
          ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("last character of a word \"%c\" is not satisfied\n", parts[i-1] )
                 )
                );
          exit(1);
        }
        /** 
         * check the one after the delimeter
         * because we already make sure last character is not dot,
         * so i + 1 should be within variable "length"
        */
        if(  isalpha( parts[ i + 1 ] ) == 0 ) {
          ereport(ERROR,
                (errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
                errmsg("start character of a word \"%c\" is not satisfied\n", parts[i+1] )
                 )
                );
          exit(1);
        }
        
        end = i - 1;
        last_end = end;
        how_many++;
        
        // if next one is still delimeter, this for loop should just end and program stops
        start = i + 1;
    }
  }
  // extract last word
  end = length - 1;
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
  return;
}

/**
 * parse string by reference
 * do not modified varaible "email" since it is read-only
 */
bool CheckEmail(char **email, char **_local, char **_domain){

    regex_t regex;
    int reti;
    char msgbuf[100]={};

    /**
     * for local and domain
     */
    size_t maxGroups = 3;
    /**
     * groupArray[0] should be exactly same with email[0]
     * groupArray[1] should contain local part without @
     * groupArray[2] should contain domain part without @
     */
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
        regfree(&regex);

        ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg("invalid email adddress, \"%s\" ", email[0])
                 )
                );
        exit(1);
    }
    else {
        regfree(&regex);

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
     * 
     * examle :
     * when input string is "jas@cse.unsw.edu.au"
     * 
     * groupArray[1].rm_so = 0,    groupArray[1].rm_eo = 3
     * groupArray[2].rm_so = 4,    groupArray[1].rm_eo = 19, where 19 is just one after the last character 'u'
     */

    if( groupArray[1].rm_eo - groupArray[1].rm_so > 256 || groupArray[2].rm_eo - groupArray[2].rm_so > 256 ) {
      regfree(&regex);
      ereport(ERROR,
				(errcode(ERRCODE_INVALID_TEXT_REPRESENTATION),
				 errmsg(">> %d >> %d >> %d >> %d\n", groupArray[1].rm_so, groupArray[1].rm_eo, 
                        groupArray[2].rm_so, groupArray[2].rm_eo)
                 )
                );
        exit(1);
    }


    char local[ groupArray[1].rm_eo - groupArray[1].rm_so + 1 ];
    char domain[ groupArray[2].rm_eo - groupArray[2].rm_so + 1 ];

    int i = 0;
    int j = 0;
    for( i = groupArray[1].rm_so ; i < groupArray[1].rm_eo ; i++ ) {
      local[j] = email[0][ i ];
      j++;
    }
    // before next line, the max possible of j is 255
    local[j] = '\0';

    j = 0;
    for( i = groupArray[2].rm_so ; i < groupArray[2].rm_eo ; i++ ) {
      domain[j] = email[0][ i ];
      j++;
    }
    // before next line, the max possible of j is 255
    domain[j] = '\0';

    int local_size = 0;
    int domain_size = 0;
    CheckParts( &local[0], &local_size, LOCAL_CHEKC_CODE );
    CheckParts( &domain[0], &domain_size, DOMAIN_CHEKC_CODE );

    _local[0] = palloc( groupArray[1].rm_eo - groupArray[1].rm_so + 1 );
    _domain[0] = palloc( groupArray[2].rm_eo - groupArray[2].rm_so + 1 );
    
    strcpy(_local[0], &local[0]);
    strcpy(_domain[0], &domain[0]);

    // free memory
    regfree(&regex);

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

  char	    *local;
  char	    *domain;

  // a series chars input, check whether it satisfy the rules
  CheckEmail( &str, &local, &domain );

  // check size is done already


  /**
   * after separate string, right now, 
   * we can know the true length of input string by using strlen() with '\0'
   * 
   * use palloc, i do not know why, but just use palloc
   * 
   * strlen() will not count '\0', so you need to + 1 
   */
  EmailAddr    *result = (EmailAddr *) palloc( VARHDRSZ + sizeof( EmailAddr )
                                        + ( strlen(local) + 1 ) 
                                        + ( strlen(domain) + 1 ) + VARHDRSZ );
  SET_VARSIZE( result , ( VARHDRSZ + sizeof( EmailAddr )
                                        + ( strlen(local) + 1 ) 
                                        + ( strlen(domain) + 1 ) + VARHDRSZ ) );
                                  
  /**
   * shoud just be perfect exactly same space incluing '\0'
   */
  int i = 0;
  for( i = 0 ; i < strlen(local) + 1 ; i++ ) {
    local[i] = tolower(local[i]);
  }
  i = 0;
  for( i = 0 ; i < strlen(domain) + 1 ; i++ ) {
    domain[i] = tolower(domain[i]);
  }
  /**
   * the palloc block is tricky
   */
  result -> first_end = strlen(local) + 1;
  i = 0;
  for( i = 0 ; i < result -> first_end ; i++ ) {
    result -> all[ i ] = local[ i ];
  }
 
  result -> second_end = result -> first_end + ( strlen(domain) + 1 );
  i = 0;
  int j = 0;
  for( i = result -> first_end ; i < result -> second_end ; i++ ) {
    result -> all[ i ] = domain[ j ];
    j++;
  }

  pfree(local);
  pfree(domain);
	PG_RETURN_POINTER(result);
}

PG_FUNCTION_INFO_V1(email_out);

Datum
email_out(PG_FUNCTION_ARGS)
{
	EmailAddr    *email = (EmailAddr *) PG_GETARG_POINTER(0);

  char *local = palloc( email -> first_end );
  char *domain = palloc( email -> second_end - email -> first_end );
	char	   *result;
  int i = 0;
  for( i = 0 ; i < email -> first_end ; i++ ) {
    local[ i ] =  email -> all[ i ];
  }
  int j = 0;
  for( i = email -> first_end ; i < email -> second_end ; i++ ) {
    domain[ j ] =  email -> all[ i ];
    j++;
  }
  
	result = psprintf("%s@%s", local, domain);
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
  char *a_local = ( char * )palloc( a -> first_end );
  char *a_domain = ( char * )palloc( a -> second_end - a -> first_end );
  int i = 0;
  for( i = 0 ; i < a -> first_end ; i++ ) {
    a_local[ i ] = a -> all[ i ];
  }
  i = 0;
  int j = 0;
  for( i = a -> first_end ; i < a -> second_end ; i++ ) {
    a_domain[ j ] = a -> all[ i ];
    j++;
  }

  char *b_local = ( char * )palloc( b -> first_end );
  char *b_domain = ( char * )palloc( b -> second_end - b -> first_end );
  i = 0;
  for( i = 0 ; i < b -> first_end ; i++ ) {
    b_local[ i ] = b -> all[ i ];
  }
  i = 0;
  j = 0;
  for( i = b -> first_end ; i < b -> second_end ; i++ ) {
    b_domain[ j ] = b -> all[ i ];
    j++;
  }
  
  // diff in domain part
  if( strcmp( a_domain, b_domain ) < 0 ) {
    return  -1;
  }
  if( strcmp( a_domain, b_domain ) > 0 ) {
    return  1;
  }

   // diff in local part
	if( strcmp( a_local, b_local ) < 0 ) {
    return  -1;
  }
  if( strcmp( a_local, b_local ) > 0 ) {
    return  1;
  }
  return 0;
}

static int
email_domain_cmp_internal(EmailAddr * a, EmailAddr * b)
{
  char *a_domain = ( char * )palloc( a -> second_end - a -> first_end );
  int i = 0;
  int j = 0;
  for( i = a -> first_end ; i < a -> second_end ; i++ ) {
    a_domain[ j ] = a -> all[ i ];
    j++;
  }

  char *b_domain = ( char * )palloc( b -> second_end - b -> first_end );
  i = 0;
  j = 0;
  for( i = b -> first_end ; i < b -> second_end ; i++ ) {
    b_domain[ j ] = b -> all[ i ];
    j++;
  }
  // diff in domain part
  if( strcmp( a_domain, b_domain ) < 0 ) {
    return  -1;
  }
  if( strcmp( a_domain, b_domain ) > 0 ) {
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

PG_FUNCTION_INFO_V1(email_address_abs_hash);

Datum
email_address_abs_hash(PG_FUNCTION_ARGS)
{
	EmailAddr *a = (EmailAddr *) PG_GETARG_POINTER(0);
	;
//	int hash = DatumGetUInt32(hash_any((const unsigned char * )a->full_address, len));

	PG_RETURN_INT32(DatumGetUInt32(hash_any((const unsigned char * )a->all, a->second_end)));
}