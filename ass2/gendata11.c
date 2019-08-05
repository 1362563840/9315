// gendata.c ... generate random tuples
// part of Multi-attribute linear-hashed files
// Generates a list of K random tuples with N attributes
// Usage:  ./insert  [-v]  #attributes  #tuples
// Last modified by John Shepherd, July 2019

#include "defs.h"

#define USAGE "./insert  #tuples  #attributes  [startID]  [seed]"

// Main ... process args, read/insert tuples

int main(int argc, char **argv)
{
	int  natts;    // number of attributes in each tuple
	int  ntups;    // number of tuples
	char err[MAXERRMSG]; // buffer for error messages

	// process command-line args

	if (argc < 3) fatal(USAGE);

	// how many tuples
	ntups = atoi(argv[1]);
	if (ntups < 1 || ntups > 100000) {
		sprintf(err, "Invalid #tuples: %d (must be 0 < # < 10^6)", ntups);
		fatal(err);
	}

	// how many attributes in each tuple
	natts = atoi(argv[2]);
	if (natts < 2 || natts > 10) {
		sprintf(err, "Invalid #attrs: %d (must be 1 < # < 11)", natts);
		fatal(err);
	}

	// reflects distribution of letter usage in english ... somewhat
	// id ensures that all tuples are distinct
	char tuple[MAXTUPLEN];
    char attr[MAXTUPLEN];
    int i;
    for (i = 0; i < ntups; i++) {
        sprintf(tuple,"%s","12");
        sprintf(attr,",%s","34");
        strcat(tuple,attr);
        sprintf(attr,",%s","56");
        strcat(tuple,attr);
        printf("%s\n",tuple);
    }
	return OK;
}
