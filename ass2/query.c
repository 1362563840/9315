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


// A suggestion ... you can change however you like
struct QueryRep {
	Reln    rel;       // need to remember Relation info
	Bits    known;     // the hash value from MAH
	Bits    unknown;   // the unknown bits from MAH
	PageID  curpage;   // current page in scan
	int     is_ovflow; // are we in the overflow pages?
	Offset  curtup;    // offset of current tuple within page
	//TODO

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
	// TODO
	// Partial algorithm:
	// form known bits from known attributes
	// form unknown bits from '?' attributes
	// compute PageID of first page
	//   using known bits and first "unknown" value
	// set all values in QueryRep object


	// check the validity of query first
	if (!queryIsValid(r, q)) {
		printf("Incorrect query, please try again.\n");
		return NULL;
	}

	// number of values in 'r'
	Count nvals = nattrs(r);
	// assign space of size 'nvals' to '**vals' (2D-array)
	char **vals = malloc(nvals * sizeof(char *));
	// assert (not necessary)
	assert(vals != NULL);
	// extract values into a 2D-array of strings
	// (char *q == Tuple t)
	tupleVals(q, vals);

	// initilize some variables
	Bits hash_value_array[nvals], the_known = 0x00000000, the_unknown = 0x00000000;
	ChVecItem *cv = chvec(r), curr_cv;	// curr_cv = cv[i]
	int the_depth = depth(r);
	PageID the_curpage = 0;

	int i;
	for (i = 0; i < nvals; i++) {
		// if value == "?", then set hash value to 0(int type)
		// else, convert value(string type) to hash value(int type)
		hash_value_array[i] = (strcmp(vals[i], "?") == 0) ? 0x00000000 : hash_any((unsigned char *)vals[i], strlen(vals[i]));
	}

	// get the_known and the_unknown
	for (i = 0; i < MAXCHVEC; i++) {
		curr_cv = cv[i];
		the_known = (bitIsSet(hash_value_array[curr_cv.att], curr_cv.bit)) ? setBit(the_known, i) : unsetBit(the_known, i);
		// if vals[curr_cv.att] == "?", then bitwise left shift
		the_unknown = (strcmp(vals[curr_cv.att], "?") == 0) ? the_unknown | (1 << i) : the_unknown;
	}

	// get the_curpage
	for (i = 0; i < the_depth; i++) {
		the_curpage = (bitIsSet(the_known, i)) ? setBit(the_curpage, i) : unsetBit(the_curpage, i);
	}

	// assign value to elements in structure 'QueryRep'
	new -> rel        =  r;
	new -> known      =  the_known;
	new -> unknown    =  the_unknown;
	new -> curpage    =  the_curpage;
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
	// TODO
	// Partial algorithm:
	// if (more tuples in current page)
	//    get next matching tuple from current page
	// else if (current page has overflow)
	//    move to overflow page
	//    grab first matching tuple from page
	// else
	//    move to "next" bucket
	//    grab first matching tuple from data page
	// endif
	// if (current page has no matching tuples)
	//    go to next page (try again)
	// endif


	// initialize
	Page current_page = (q -> is_ovflow == 0) ? getPage(dataFile(q -> rel), q -> curpage) : getPage(ovflowFile(q -> rel), q -> curpage);
	Count number_of_tuples = pageNTuples(current_page);
	// update q -> str_data if current_page changed
	if (q -> is_ovflow == 1  ||  strcmp(q -> str_data, "EmptyString") == 0) {
		// q->str_data should include data
		q -> str_data = pageData(current_page);
	}

	// ensure length of q->str_data > 0
	// i.e. q->str_data should not be ""
	// i.e. q->str_data == "SOME DATA"
	if (tupLength(q -> str_data) == 0) {
		return NULL;
	}
	
	// # 1 ------ if (more tuples in current page)
	// scan current page
	for (int i = 0; i < number_of_tuples; i++) {
		// successful match
		// get the required tuple
		// then, move pointer to point the next data
		// lastly, return this required tuple
		if (tupleMatch(q -> rel, q -> str_query, q -> str_data) == TRUE) {
			Tuple required_tuple = q -> str_data;
			q -> str_data += tupLength(q -> str_data) + 1;
			return required_tuple;
		}
		// unsuccessful match
		// just move pointer to point the next data
		else {
			q -> str_data += tupLength(q -> str_data) + 1;
		}


		// # 2 ------ else if (current page has overflow)
		// scan overflow
		if (pageOvflow(current_page) != NO_PAGE) {
			// jump to the overflow page
			q->curpage = pageOvflow(current_page);
			q -> is_ovflow = 1;
		}
		// # 3 ------ else, move to "next" bucket
		// jump to next page
		else {
			q -> curpage += (1 << q -> int_depth);
			q -> is_ovflow = 0;
		}


		// # 4 ------ if (current page has no matching tuples)
		// end loop [ in select.c, line 57: while ((t = getNextTuple(q)) != NULL) ]
		if (q -> is_ovflow == NO_PAGE) {
			return NULL;
		}

	}
	

	return NULL;
}


// clean up a QueryRep object and associated data
void closeQuery(Query q)
{
	// TODO

	// struct 'q' has allocated memory using 'malloc'
	// thus, free 'q'
	free(q);
}


