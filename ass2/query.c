// query.c ... query scan functions
// part of Multi-attribute Linear-hashed Files
// Manage creating and using Query objects
// Last modified by John Shepherd, July 2019


#include "defs.h"
#include "query.h"
#include "reln.h"
#include "tuple.h"

// include 2 more .h file
#include "bits.h"
#include "hash.h"

Bool moveToNextPage( Query _q, Page _current_page );
char * readtupleInQuery( char * start, char * end );
Bool checkValidHashBit( PageID _currMainPageID, int _how_many_bits, Bits _known, Bits _known_pos );

// A suggestion ... you can change however you like
struct QueryRep {
	Reln    rel;       // need to remember Relation info
	Bits    known;     // the hash value from MAH
	Bits    known_pos;   // which pos is known
	PageID  curMainPage;   // current main page in scan
	PageID  curOvPage;	
	int     is_ovflow; // are we in the overflow pages?
	Offset  curtup;    // offset of current tuple within page

	int  int_depth;		// depth (constant)
	char *str_query;	// query (constant)
	char *str_data;		// data in tuple (variable)

};



// check the validity of query
// i.e. check whether number of att in 'q' == nattrs(r)
int queryIsValid(Reln r, char *q)
{
	int i;
	Count count = 1;
	for (i = 0; q[i]; i++) {
		count += (q[i] == ',');
	}
	return (count == nattrs(r));
}


// take a query string (e.g. "1234,?,abc,?")
// set up a QueryRep object for the scan
Query startQuery(Reln r, char *q)
{
	Query new = malloc(sizeof(struct QueryRep));
	assert(new != NULL);

	// check the validity of query first
	if (!queryIsValid(r, q)) {
		printf("Incorrect query, please try again.\n");
		return NULL;
	}

	// number of values in 'r'
	Count nvals = nattrs(r);
	// assign space of size 'nvals' to '**vals' (2D-array)
	char **vals = malloc(nvals * sizeof(char *));
	// assert check if malloc fails
	assert(vals != NULL);
	// extract values into a 2D-array of strings
	// (char *q == Tuple t)
	tupleVals(q, vals);

	// initilize some variables
	Bits hash_value_array[nvals], the_known = 0x00000000, temp_pos = 0x00000000;
	ChVecItem *cv = chvec(r);
	ChVecItem curr_cv;	// curr_cv = cv[i]
	int the_depth = depth(r);

	int i;
	for (i = 0; i < nvals; i++) {
		// if value == "?", then set hash value to 0(int type)
		// else, convert value(string type) to hash value(int type)
		hash_value_array[i] = (strcmp(vals[i], "?") == 0) ? 0x00000000 : hash_any((unsigned char *)vals[i], strlen(vals[i]));
	}

	// get the_known and the_unknown
	for (i = 0; i < MAXCHVEC; i++) {
		curr_cv = cv[i];
		the_known = ( bitIsSet(hash_value_array[curr_cv.att], curr_cv.bit) ) ? setBit(the_known, i) : unsetBit(the_known, i);
		// if vals[curr_cv.att] == "?", then bitwise left shift
		temp_pos = (strcmp(vals[curr_cv.att], "?") != 0) ? temp_pos | (1 << i) : temp_pos;
	}
	void bitsString(Bits val, char *buf);

	// assign value to elements in structure 'QueryRep'
	new -> rel        =  r;
	new -> known      =  the_known;
	new -> known_pos  =  temp_pos;
	new -> curMainPage=  0; // possible bug
	new -> curOvPage  =  NO_PAGE;
	new -> is_ovflow  =  0;
	new -> curtup     =  0;
	new -> int_depth  =  the_depth;
	new -> str_query  =  q;
	new -> str_data   =  "EmptyString";

	// free 'vals' because it has allocated memory using 'malloc'
	freeVals(vals, nvals);

	// return struct 'new' which is initialized
	return new;
}


// get next tuple during a scan
Tuple getNextTuple(Query q)
{	
	Page current_page;
	// initialize
	if( q->is_ovflow == 0 ) {
		current_page = getPage(dataFile(q->rel), q->curMainPage);
	}
	else{
		current_page = getPage(ovflowFile(q->rel), q->curOvPage);
	}

	// update q->str_data if current_page changed
	// get tuple parts start at rel->data[0]
	q->str_data = pageData(current_page);

	// set the inistal pos to right offset
	// initial can only change when reading another ov or main page
	char *initial = q->str_data + q->curtup;
	char *start = initial;

	// this page's all tuples are checked
	char *end = NULL;
	char *last_end = NULL;
	char *curr = start;
	for( ; ; ) {
		if( *curr != '\0' && start != NULL ) {
			q->curtup++;
			curr = curr + 1;
			continue;
		}	
		
		// a new tuple start
		if( *curr != '\0' && start == NULL ) {
			q->curtup++;
			start = curr;
			curr = curr + 1;
			continue;
		}

		// a tuple ends
		if( *curr == '\0' && start != NULL && curr != initial ) {
			q->curtup++;
			end = curr;
			Tuple resultTuple = readtupleInQuery( start, end );
			Bool matchResult = tupleMatch( q->rel, q->str_query, resultTuple );
			// if matches
			if( matchResult == TRUE ) {
				free(current_page);
				return resultTuple;
			}
			start = NULL;
			end = NULL;
			last_end = curr;
			curr = curr + 1;
			continue;
		}

		// this page no longer has tuples from current pos
		// or no tuple at all
		if( *curr == '\0' && curr == initial ) {
			Bool temp_result = moveToNextPage( q, current_page );
			if( temp_result == FALSE ){
				// moveToNextPage() help us free(current_page)
				// free(current_page);
				break;
			}
			initial = q->str_data;
			start = initial;
			curr = start;
			continue;
		}

		// all tuples in current page is read
		if( *curr == '\0' && last_end + 1 == curr ) {
			Bool temp_result = moveToNextPage( q, current_page );
			if( temp_result == FALSE ){
				// moveToNextPage() help us free(current_page)
				// free(current_page);
				break;
			}
			initial = q->str_data;
			start = initial;
			curr = start;
			last_end = NULL;
			continue;
		}
		
	}
	return NULL;
}

/**
 * 1. from main page to its ov
 * 2. from main page to next main
 * 3. from ov to next main
 * 4. from ov to ov
 */
Bool moveToNextPage( Query _q, Page _current_page )
{
	// 1 or 4
	if( pageOvflow(_current_page) != NO_PAGE ) {
		_q->curOvPage = pageOvflow(_current_page);
		free(_current_page);
		_current_page = getPage( ovflowFile(_q->rel), _q->curOvPage );
		_q->str_data = pageData(_current_page);
		
		_q->is_ovflow = 1;
		_q->curtup = 0;
	}
	// 2 or 3
	// read next main page or finished
	else{
		// all main and ov pages are done
		if( _q->curMainPage == int_pow( 2, _q->int_depth ) - 1 + splitp(_q->rel) ) {
			return FALSE;
		}
		else{
			_q->curMainPage++;
			for( ; _q->curMainPage <= int_pow( 2, _q->int_depth ) - 1 + splitp(_q->rel) ; _q->curMainPage++ ){
				/**
				 * check if new _q->curMainPage is valid for known bits
				 * 1. _q->curMainPage < _q->rel->sp, take depth + 1 bits
				 * 2. _q->curMainPage >= _q->curMainPage && < 2^depth
				 * 3. _q->curMainPage >= 2^depth && assert(_q->curMainPage < int_pow( 2, _q->int_depth ) - 1 + splitp(_q->rel))
			 	*/ 
				if( _q->curMainPage < _q->int_depth ) {
					Bool temp_result = checkValidHashBit( _q->curMainPage, _q->int_depth + 1, _q->known, _q->known_pos );
					if( temp_result == FALSE ) {
						continue;
					}
					break;
				}

				if( _q->curMainPage >= _q->int_depth && _q->curMainPage < int_pow(2,_q->int_depth) ) {
					Bool temp_result = checkValidHashBit( _q->curMainPage, _q->int_depth, _q->known, _q->known_pos );
					if( temp_result == FALSE ) {
						continue;
					}
					break;
				}
				if( _q->curMainPage >= int_pow(2,_q->int_depth) ) {
					Bool temp_result = checkValidHashBit( _q->curMainPage, _q->int_depth + 1, _q->known, _q->known_pos );
					if( temp_result == FALSE ) {
						continue;
					}
					break;
				}
				/**
				 * Attention, do not delete, if programs run here, then it means 
		 		*/ 
				assert( 1 == 0 );
			}
			if( _q->curMainPage > int_pow( 2, _q->int_depth ) - 1 + splitp(_q->rel) ) {
				free(_current_page);
				return FALSE;
			}
			_q->curOvPage = NO_PAGE;
			free(_current_page);
			_current_page = getPage( dataFile(_q->rel), _q->curMainPage );
			_q->str_data = pageData(_current_page);

			_q->is_ovflow = 0;
			_q->curtup = 0;
 		}
	}
	return TRUE;
}


char * readtupleInQuery( char * start, char * end )
{
	char * result = malloc( sizeof( char ) * ( end - start + 1 ) );
	if( result == NULL ) {
		printf("not enough space for malloc\n");
		exit(1);
	}
	char * curr = start;
	int i = 0;
	for(  ; curr <= end ; ) {
		result[ i ] = *curr;
		curr = curr + 1;
		i++;
	}
	return result;
}

Bool checkValidHashBit( PageID _currMainPageID, int _how_many_bits, Bits _known, Bits _known_pos )
{
	for( int i = 0 ; i < _how_many_bits ; i++ ) {
		// this pos is known
		if( extractBit( _known_pos, i ) == 1 ) {
			if( extractBit( _currMainPageID, i ) != extractBit( _known, i ) ) {
				return FALSE;
			}
		}
	}
	return TRUE;
}

// clean up a QueryRep object and associated data
void closeQuery(Query q)
{
	free(q);
}
