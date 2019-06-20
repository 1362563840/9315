/*
 * src/tutorial/complex.c
 *
 ******************************************************************************
  This file contains routines that can be bound to a Postgres backend and
  called by the backend in the process of processing queries.  The calling
  format for these routines is dictated by Postgres architecture.
******************************************************************************/

#include "postgres.h"

#include "fmgr.h"
#include "libpq/pqformat.h"		/* needed for send/recv functions */

#include <regex.h>
#include <stdlib.h>
#include <string.h>
bool CheckEmail(char **email, char **_local, char **_domain);

PG_MODULE_MAGIC;

typedef struct EmailAddr 
{
    char local[ 128 + 1 ];
    char domain[ 128 + 1 ]; 
} EmailAddr;


/**
 * parse string by reference
 * do not modified varaible "email" since it is read-only 
 */
bool CheckEmail(char **email, char **_local, char **_domain){

    regex_t regex;
    int reti;
    char msgbuf[100]={};
    // char original[257];
    char local[ 128 + 1 ]={};
    char domain[ 128 + 1 ]={};

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
        fprintf(stderr, "Could not compile regex\n");
        exit(1);
    }

    /* Execute regular expression */
    reti = regexec(&regex, email[0], maxGroups, groupArray, 0);
    if ( reti == 0 ) {
        puts("Match\n");
    }
    else if (reti == REG_NOMATCH) {
        puts("No match\n");
    }
    else {
        regerror(reti, &regex, msgbuf, sizeof(msgbuf));
        fprintf(stderr, "Regex match failed: %s\n", msgbuf);
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
    // move to start position, it should be zero
    temp_copy_email = temp_copy_email + groupArray[2].rm_so;
    strcpy( &domain[0], temp_copy_email );
    //

    printf("local is %s, domain is %s\n", local, domain);

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
	char	    *local = (char *)malloc( sizeof(char) * ( 128 + 1 ) );
    local[128] = '\0';
    char	    *domain = (char *)malloc( sizeof(char) * ( 128 + 1 ) );
    domain[128] = '\0';
	EmailAddr    *result = (EmailAddr *) palloc(sizeof(EmailAddr));
    // initialize
    result -> local[128] = '\0';
    result -> domain[128] = '\0';
    // a series chars input, check whether it satisfy the rules
    CheckEmail( &str, &local, &domain );

    strcpy(result -> local, local);
    strcpy(result -> domain, domain);
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

// /*****************************************************************************
//  * Binary Input/Output functions
//  *
//  * These are optional.
//  *****************************************************************************/

// PG_FUNCTION_INFO_V1(complex_recv);

// Datum
// complex_recv(PG_FUNCTION_ARGS)
// {
// 	StringInfo	buf = (StringInfo) PG_GETARG_POINTER(0);
// 	Complex    *result;

// 	result = (Complex *) palloc(sizeof(Complex));
// 	result->x = pq_getmsgfloat8(buf);
// 	result->y = pq_getmsgfloat8(buf);
// 	PG_RETURN_POINTER(result);
// }

// PG_FUNCTION_INFO_V1(complex_send);

// Datum
// complex_send(PG_FUNCTION_ARGS)
// {
// 	Complex    *complex = (Complex *) PG_GETARG_POINTER(0);
// 	StringInfoData buf;

// 	pq_begintypsend(&buf);
// 	pq_sendfloat8(&buf, complex->x);
// 	pq_sendfloat8(&buf, complex->y);
// 	PG_RETURN_BYTEA_P(pq_endtypsend(&buf));
// }

// /*****************************************************************************
//  * New Operators
//  *
//  * A practical Complex datatype would provide much more than this, of course.
//  *****************************************************************************/

// PG_FUNCTION_INFO_V1(complex_add);

// Datum
// complex_add(PG_FUNCTION_ARGS)
// {
// 	Complex    *a = (Complex *) PG_GETARG_POINTER(0);
// 	Complex    *b = (Complex *) PG_GETARG_POINTER(1);
// 	Complex    *result;

// 	result = (Complex *) palloc(sizeof(Complex));
// 	result->x = a->x + b->x;
// 	result->y = a->y + b->y;
// 	PG_RETURN_POINTER(result);
// }


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

// #define Mag(c)	((c)->x*(c)->x + (c)->y*(c)->y)

// static int
// complex_abs_cmp_internal(Complex * a, Complex * b)
// {
// 	double		amag = Mag(a),
// 				bmag = Mag(b);

// 	if (amag < bmag)
// 		return -1;
// 	if (amag > bmag)
// 		return 1;
// 	return 0;
// }


// PG_FUNCTION_INFO_V1(complex_abs_lt);

// Datum
// complex_abs_lt(PG_FUNCTION_ARGS)
// {
// 	Complex    *a = (Complex *) PG_GETARG_POINTER(0);
// 	Complex    *b = (Complex *) PG_GETARG_POINTER(1);

// 	PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) < 0);
// }

// PG_FUNCTION_INFO_V1(complex_abs_le);

// Datum
// complex_abs_le(PG_FUNCTION_ARGS)
// {
// 	Complex    *a = (Complex *) PG_GETARG_POINTER(0);
// 	Complex    *b = (Complex *) PG_GETARG_POINTER(1);

// 	PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) <= 0);
// }

// PG_FUNCTION_INFO_V1(complex_abs_eq);

// Datum
// complex_abs_eq(PG_FUNCTION_ARGS)
// {
// 	Complex    *a = (Complex *) PG_GETARG_POINTER(0);
// 	Complex    *b = (Complex *) PG_GETARG_POINTER(1);

// 	PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) == 0);
// }

// PG_FUNCTION_INFO_V1(complex_abs_ge);

// Datum
// complex_abs_ge(PG_FUNCTION_ARGS)
// {
// 	Complex    *a = (Complex *) PG_GETARG_POINTER(0);
// 	Complex    *b = (Complex *) PG_GETARG_POINTER(1);

// 	PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) >= 0);
// }

// PG_FUNCTION_INFO_V1(complex_abs_gt);

// Datum
// complex_abs_gt(PG_FUNCTION_ARGS)
// {
// 	Complex    *a = (Complex *) PG_GETARG_POINTER(0);
// 	Complex    *b = (Complex *) PG_GETARG_POINTER(1);

// 	PG_RETURN_BOOL(complex_abs_cmp_internal(a, b) > 0);
// }

// PG_FUNCTION_INFO_V1(complex_abs_cmp);

// Datum
// complex_abs_cmp(PG_FUNCTION_ARGS)
// {
// 	Complex    *a = (Complex *) PG_GETARG_POINTER(0);
// 	Complex    *b = (Complex *) PG_GETARG_POINTER(1);

// 	PG_RETURN_INT32(complex_abs_cmp_internal(a, b));
// }

