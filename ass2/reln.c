// reln.c ... functions on Relations
// part of Multi-attribute Linear-hashed Files
// Last modified by John Shepherd, July 2019

#include "defs.h"
#include "reln.h"
#include "page.h"
#include "tuple.h"
#include "chvec.h"
#include "bits.h"
#include "hash.h"

#include <string.h>

#define HEADERSIZE (3*sizeof(Count)+sizeof(Offset))

struct RelnRep {
	Count  nattrs; // number of attributes
	Count  depth;  // depth of main data file
	/**
	 * sp just points block's value,
	 * In lecture 04 P95
	 * 0    1   2   3   4
	 * 000, 01, 10, 11, 100
	 * 	 sp:^
	 * take the last bit of a hash value, if it is 00, then it is possbile that it belongs to the last block(4)
	 * so, need to take one more bit. Originally, it is 2 bits, then 3 bits
	 */
	Offset sp;     // split pointer	
    Count  npages; // number of main data pages
    Count  ntups;  // total number of tuples

	/**
	 * I store the first empty pageId,
	 * The remaining is accessed by getPage(r->data, r->first_empty_page)
	 * PageID nextPid = getPage(r->data, r->first_empty_page);
	 * while( nextPid != NO_PAGE ) {
	 * 	nextPid = getPage(r->data, nextPid);
	 * }
	 */
	PageID first_empty_page;

	ChVec  cv;     // choice vector
	/**
	 * r means read only
	 * w means need to write before close
	 */
	char   mode;   // open for read/write
	FILE  *info;   // handle on info file
	FILE  *data;   // handle on data file
	FILE  *ovflow; // handle on ovflow file
};

// create a new relation (three files)

Status newRelation(char *name, Count nattrs, Count npages, Count d, char *cv)
{
    char fname[MAXFILENAME];
	Reln r = malloc(sizeof(struct RelnRep));
	r->nattrs = nattrs; r->depth = d; r->sp = 0;
	r->npages = npages; r->ntups = 0; r->mode = 'w'; r->first_empty_page = NO_PAGE;
	assert(r != NULL);
	if (parseChVec(r, cv, r->cv) != OK) return ~OK;
	sprintf(fname,"%s.info",name);
	r->info = fopen(fname,"w");
	assert(r->info != NULL);
	sprintf(fname,"%s.data",name);
	r->data = fopen(fname,"w");
	assert(r->data != NULL);
	sprintf(fname,"%s.ovflow",name);
	r->ovflow = fopen(fname,"w");
	assert(r->ovflow != NULL);
	int i;
	for (i = 0; i < npages; i++) addPage(r->data);
	// closeRelation() writes global info
	closeRelation(r);
	return 0;
}

// check whether a relation already exists

Bool existsRelation(char *name)
{
	char fname[MAXFILENAME];
	sprintf(fname,"%s.info",name);
	FILE *f = fopen(fname,"r");
	if (f == NULL)
		return FALSE;
	else {
		fclose(f);
		return TRUE;
	}
}

// set up a relation descriptor from relation name
// open files, reads information from rel.info

Reln openRelation(char *name, char *mode)
{
	Reln r;
	r = malloc(sizeof(struct RelnRep));
	assert(r != NULL);
	char fname[MAXFILENAME];
	sprintf(fname,"%s.info",name);
	r->info = fopen(fname,mode);
	assert(r->info != NULL);
	sprintf(fname,"%s.data",name);
	r->data = fopen(fname,mode);
	assert(r->data != NULL);
	sprintf(fname,"%s.ovflow",name);
	r->ovflow = fopen(fname,mode);
	assert(r->ovflow != NULL);
	// Naughty: assumes Count and Offset are the same size
	int n = fread(r, sizeof(Count), 6, r->info);
	assert(n == 6);
	n = fread(r->cv, sizeof(ChVecItem), MAXCHVEC, r->info);
	assert(n == MAXCHVEC);
	r->mode = (mode[0] == 'w' || mode[1] =='+') ? 'w' : 'r';
	return r;
}

// release files and descriptor for an open relation
// copy latest information to .info file

void closeRelation(Reln r)
{
	// make sure updated global data is put in info
	// Naughty: assumes Count and Offset are the same size
	if (r->mode == 'w') {
		fseek(r->info, 0, SEEK_SET);
		// write out core relation info (#attr,#pages,d,sp)
		int n = fwrite(r, sizeof(Count), 6, r->info);
		assert(n == 6);
		// write out choice vector
		n = fwrite(r->cv, sizeof(ChVecItem), MAXCHVEC, r->info);
		assert(n == MAXCHVEC);
	}
	fclose(r->info);
	fclose(r->data);
	fclose(r->ovflow);
	free(r);
}

freeBackup( char **backup, int how_many_tuples ) {
	for( int i = 0; i < how_many_tuples ; i++ ) {
		free( backup[ i ] );
	}
	free( backup );
}

/**
 * 															including	including
 * 																	*end = '\0'
 */
BackTuple( char **backup, int *how_many_existing_tuples, char *start, char *end )
{											//
	char *temp = malloc( sizeof( char ) * ( end - start + 1 ) );
	char *offset = start;
	for( int i = 0 ; offset <= end ; i++ ) {
		temp[ i ] = *offset;
		offset = offset + 1;
	}
	backup
}

/**
 * According to forum 
 * We first read all tuples and store it in in-memory
 * Then reset page pointed by sp
 * Then insert again
 */

Bool SplitPage( Reln _r )
{	
	// create a new page
	PageID newPid = addPage( _r->data );	
	_r->npages++;

	// test if pid is same as theory;
	Offset temp = _r->sp | ( 1 << _r->depth );
	assert( newPid == temp );

	// split tuples in the page pointed by sp;
	Page target_page = getPage( _r->data, _rr->sp );
	const Count hdr_size = 2*sizeof(Offset) + sizeof(Count); // make it const
	int current_tuple = 0;
	/***
	 * 1 is for " end of string "
	 */
	Tuple temp_tuple = calloc( MAXTUPLEN + 1, sizeof( char ) ); 
	int tuple_length = 0;

	// Count total_tuples_num = 0;

	PageID curr_pid = _r->sp;

	// reset to use again
	curr_pid = _r->sp;
	// after knowing the space, create 2d char
	
	for( ; curr_pid != NO_PAGE ; ) {
		/**
		 * Attention, need to free curr_page each time you use
		 */
		Page curr_page = getPage( _r->data, curr_pid );
		Count how_many_tuples_curr_page = pageNTuples( curr_page );
		Count existing_scanned_tuples_num = 0;
		// total_tuples_num = total_tuples_num + how_many_tuples_curr_page;
		
		Count how_much_space_curr_page = pageFreeSpace( curr_page );

		char * const initial = pageData( curr_page );

		// no tuple at this page(main/overflow)
		if( how_many_tuples_curr_page == 0 ) {
			assert( *(initial) == '\0' );
			curr_pid = pageOvflow( curr_page );
			continue;
		}

		char *start = NULL;
		char *end = NULL;
		char *last_end = NULL;

		char **backup = malloc( sizeof( char * ) * how_many_tuples_curr_page );
		// for current page, extract all tuples and store
		for( int i = 0 ; i < ( PAGESIZE - hdr_size ) ; i++ ) {
			// most possible situation, reading a tuple
												//		&& end == NULl // Attention
			if( *(initial + i) != '\0' && start != NULL ) {
				// temp_tuple[ tuple_length ] = *(initial + i);
				// tuple_length++;
				continue;
			}

			// start of a tuple
			if( *(initial + i) != '\0' && start == NULL ) {
				/**
				 * Attention : These asserts can be deleted to increase speed
				 */
				// assert( end == NULL && tuple_length == 0 );
				assert( end == NULL );
				// temp_tuple[ tuple_length ] = *(initial + i);
				start = initial + i;
			}
			// end of a tuple					//		&& end == NULl // Attention
			if( *(initial + i) == '\0' && start != NULL ) {
				end = initial + i;
				// store this tuple	into backup
				BackTuple( backup, &existing_scanned_tuples_num, start, end );
				// after store finished, reset
				last_end = end;
				memset( temp_tuple, 0, tuple_length );
				tuple_length = 0;
				start = NULL;
				end = NULL;
				continue;
			}

			// last possible, the previous tuple is the last tuple
			// Thic condition can be satisfied 
			// because if no tuple at all, then this for loop should never happen
			assert( last_end != NULL );
			if( *(initial + i) == '\0' &&  start == NULL && ( initial + i ) == last_end + 1 ) {
				break;
			}

		}
		// after store
		// reset this page
		// insert again, except this time you use one more bit of hash value
		freeBackup(backup);
		curr_pid = pageOvflow( curr_page );
	}



	for( int i = 0 ; i < ( PAGESIZE-hdr_size ) ; i++ ) {
		// if currently it is still part of tuple, copy it;
		if( start != NULL && end == NULL ) {
			tuple[ tuple_length ] = *( initial + i );
			tuple_length++;
			continue;	// avoiding below if to save speed
		}
		// find a tuple start offset
		if( *(initial + i) != '\0' && start == NULL ) {
			start = initial + i;
			continue;	// avoiding below if to save speed
		}
		// find a tuple end offset
		if( *(initial + i) == '\0' && start != NULL ) {
			assert( end == NULL );
			end = initial + i;
			//-------------------------------------------- these two lines should not matter, since default char fot each
			//												index is '\0'(end of string)				
			// tuple[ tuple_length ] = '\0';
			// tuple_length++;	
			//--------------------------------------------
			// TODO
			// rehash
			Bits tuple_hash_value = tupleHash( _r, tuple);
			Bits tuple_hash_bits = getLower( tuple_hash_value, _r->depth+1 ); // take the lower (depth + 1) bits


			// after finish redisturibute, reset things
			start = NULL;
			end = NULL;
			memset( tuple, 0, tuple_length );
			tuple_length = 0;
		}
	}
}

/**
 * If a overflow A is going to be added to an existing overflow page B,
 * then B.ovflow will record the pid of A.
 * If need to access overflow page, then just use pid * PAGESIZE to find its position
 */
// insert a new tuple into a relation
// returns index of bucket where inserted
// - index always refers to a primary data page
// - the actual insertion page may be either a data page or an overflow page
// returns NO_PAGE if insert fails completely
// TODO: include splitting and file expansion

PageID addToRelation(Reln r, Tuple t)
{
	Bits h, p;
	// char buf[MAXBITS+1];
	h = tupleHash(r,t);
	if (r->depth == 0)
		p = 1;
	else {
		p = getLower(h, r->depth);
		if (p < r->sp) p = getLower(h, r->depth+1);
	}
	// bitsString(h,buf); printf("hash = %s\n",buf);
	// bitsString(p,buf); printf("page = %s\n",buf);
	Page pg = getPage(r->data,p);
	if (addToPage(pg,t) == OK) {
		putPage(r->data,p,pg);
		r->ntups++;
		return p;
	}
	// primary data page full
	/**
	 * no overflow page
	 */
	if (pageOvflow(pg) == NO_PAGE) {
		// add first overflow page in chain

		// create a new overflow page 
		PageID newp = addPage(r->ovflow);
		// set this page as overflow page of existing primary page(pg)
		pageSetOvflow(pg,newp);
		putPage(r->data,p,pg);
		Page newpg = getPage(r->ovflow,newp);
		// can't add to a new overflow page; we have a problem
		if (addToPage(newpg,t) != OK) return NO_PAGE;
		putPage(r->ovflow,newp,newpg);
		r->ntups++;
		return p;
	}
	else {
		// scan overflow chain until we find space
		// worst case: add new ovflow page at end of chain
		Page ovpg, prevpg = NULL;
		PageID ovp, prevp = NO_PAGE;
		ovp = pageOvflow(pg);
		while (ovp != NO_PAGE) {
			ovpg = getPage(r->ovflow, ovp);
			if (addToPage(ovpg,t) != OK) {
				prevp = ovp; prevpg = ovpg;
				ovp = pageOvflow(ovpg);
			}
			else {
				if (prevpg != NULL) free(prevpg);
				putPage(r->ovflow,ovp,ovpg);
				r->ntups++;
				return p;
			}
		}
		// all overflow pages are full; add another to chain
		// at this point, there *must* be a prevpg
		assert(prevpg != NULL);
		// make new ovflow page
		PageID newp = addPage(r->ovflow);
		// insert tuple into new page
		Page newpg = getPage(r->ovflow,newp);
        if (addToPage(newpg,t) != OK) return NO_PAGE;
        putPage(r->ovflow,newp,newpg);
		// link to existing overflow chain
		pageSetOvflow(prevpg,newp);
		putPage(r->ovflow,prevp,prevpg);
        r->ntups++;
		return p;
	}
	
	return NO_PAGE;
}

PageID addToRelationSplitVersion(Reln r, Tuple t)
{
	Bits h, p;
	// char buf[MAXBITS+1];
	h = tupleHash(r,t);
	p = getLower(h, r->depth+1);

	// bitsString(h,buf); printf("hash = %s\n",buf);
	// bitsString(p,buf); printf("page = %s\n",buf);
	Page pg = getPage(r->data,p);
	if (addToPage(pg,t) == OK) {
		putPage(r->data,p,pg);
		// r->ntups++;
		return p;
	}
	// primary data page full
	/**
	 * no overflow page
	 */
	if (pageOvflow(pg) == NO_PAGE) {
		// add first overflow page in chain

		// create a new overflow page 
		PageID newp = addPage(r->ovflow);
		// set this page as overflow page of existing primary page(pg)
		pageSetOvflow(pg,newp);
		putPage(r->data,p,pg);
		Page newpg = getPage(r->ovflow,newp);
		// can't add to a new overflow page; we have a problem
		if (addToPage(newpg,t) != OK) return NO_PAGE;
		putPage(r->ovflow,newp,newpg);
		// r->ntups++;
		return p;
	}
	else {
		// scan overflow chain until we find space
		// worst case: add new ovflow page at end of chain
		Page ovpg, prevpg = NULL;
		PageID ovp, prevp = NO_PAGE;
		ovp = pageOvflow(pg);
		while (ovp != NO_PAGE) {
			ovpg = getPage(r->ovflow, ovp);
			if (addToPage(ovpg,t) != OK) {
				prevp = ovp; prevpg = ovpg;
				ovp = pageOvflow(ovpg);
			}
			else {
				if (prevpg != NULL) free(prevpg);
				putPage(r->ovflow,ovp,ovpg);
				// r->ntups++;
				return p;
			}
		}
		// all overflow pages are full; add another to chain
		// at this point, there *must* be a prevpg
		assert(prevpg != NULL);
		// make new ovflow page
		PageID newp = addPage(r->ovflow);
		// insert tuple into new page
		Page newpg = getPage(r->ovflow,newp);
        if (addToPage(newpg,t) != OK) return NO_PAGE;
        putPage(r->ovflow,newp,newpg);
		// link to existing overflow chain
		pageSetOvflow(prevpg,newp);
		putPage(r->ovflow,prevp,prevpg);
        // r->ntups++;
		return p;
	}
	
	return NO_PAGE;
}

// external interfaces for Reln data

FILE *dataFile(Reln r) { return r->data; }
FILE *ovflowFile(Reln r) { return r->ovflow; }
Count nattrs(Reln r) { return r->nattrs; }
Count npages(Reln r) { return r->npages; }
Count ntuples(Reln r) { return r->ntups; }
Count depth(Reln r)  { return r->depth; }
Count splitp(Reln r) { return r->sp; }
ChVecItem *chvec(Reln r)  { return r->cv; }


// displays info about open Reln

void relationStats(Reln r)
{
	printf("Global Info:\n");
	printf("#attrs:%d  #pages:%d  #tuples:%d  d:%d  sp:%d\n",
	       r->nattrs, r->npages, r->ntups, r->depth, r->sp);
	printf("Choice vector\n");
	printChVec(r->cv);
	printf("Bucket Info:\n");
	printf("%-4s %s\n","#","Info on pages in bucket");
	printf("%-4s %s\n","","(pageID,#tuples,freebytes,ovflow)");
	for (Offset pid = 0; pid < r->npages; pid++) {
		printf("[%2d]  ",pid);
		Page p = getPage(r->data, pid);
		Count ntups = pageNTuples(p);
		Count space = pageFreeSpace(p);
		Offset ovid = pageOvflow(p);
		printf("(d%d,%d,%d,%d)",pid,ntups,space,ovid);
		free(p);
		while (ovid != NO_PAGE) {
			Offset curid = ovid;
			p = getPage(r->ovflow, ovid);
			ntups = pageNTuples(p);
			space = pageFreeSpace(p);
			ovid = pageOvflow(p);
			printf(" -> (ov%d,%d,%d,%d)",curid,ntups,space,ovid);
			free(p);
		}
		putchar('\n');
	}
}
