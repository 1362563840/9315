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
	 * First overflow page, not main page
	 * 
	 * I store the first empty pageId,
	 * The remaining is accessed by getPage(r->ovflow, r->first_empty_page)
	 * PageID nextOvPid = getPage(r->ovflow, r->first_empty_page);
	 * while( nextOvPid != NO_PAGE ) {
	 * 	nextOvPid = getPage(r->ovflow, nextOvPid);
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

void Store_And_insert_agian( File *_handler, PageID _pid, Reln _r ) 
{
	const Count hdr_size = 2*sizeof(Offset) + sizeof(Count); // make it const

	Page curr_page = getPage( _handler, _pid );
	Count how_many_tuples_curr_page = pageNTuples( curr_page );
	Count existing_scanned_tuples_num = 0;

	char * const initial = pageData( curr_page );

	// no tuple at this page(main/overflow)
	if( how_many_tuples_curr_page == 0 ) {
		assert( *(initial) == '\0' );
		free( curr_page );
		return;
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
			/**
			 * Attention : These asserts can be deleted to increase speed
			 */
			assert( end == NULL );
			continue;
		}

		// start of a tuple
		if( *(initial + i) != '\0' && start == NULL ) {
			/**
			 * Attention : These asserts can be deleted to increase speed
			 */
			assert( end == NULL );
			start = initial + i;
			continue;
		}

		// end of a tuple					//		&& end == NULl // Attention
		if( *(initial + i) == '\0' && start != NULL ) {
			end = initial + i;
			// store this tuple	into backup
			BackTuple( backup, &existing_scanned_tuples_num, start, end );
			// count tuple nums by + 1
			existing_scanned_tuples_num++;
			// after store finished, reset
			last_end = end;
			start = NULL;
			end = NULL;
			continue;
		}

		// last possible, the previous tuple is the last tuple
		// Thic condition can be satisfied 
		// because if no tuple at all, then this for loop should never happen
		assert( last_end != NULL );
		if( *(initial + i) == '\0' &&  start == NULL && ( initial + i ) == last_end + 1 ) {
			/**
			 * Attention : These asserts can be deleted to increase speed
			 */
			assert( existing_scanned_tuples_num == how_many_tuples_curr_page );
			break;
		}

	}

	// after store
	// reset this page's 3 members, the last one data[1] should be same
	// reset the tuple parts

	resetPageInfo( _handler, curr_pid, curr_page );

	// insert again, except this time you use one more bit of hash value
	for( int i = 0 ; i < how_many_tuples_curr_page ; i++ ) {
		addToRelationSplitVersion( _r, backup[ i ] );
	}
	// free char **backup, curr_page
	freeBackup(backup);
	/**
	 * Attention 
	 * 
	 * in resetPageInfo(), it calls putPage(), putPage() will call free()
	 */
	// free( curr_page );

	return;
}

/**
 * According to forum 
 * We first read all tuples and store it in in-memory
 * Then reset page pointed by sp
 * Then insert again
 */
Bool SplitPage( Reln _r )
{	
	// create a new main page
	PageID newPid = addPage( _r->data );	
	_r->npages++;
	
	/**
	 * Attention, test assert, can be deleted
	 */
	// test if pid is same as theory;
	Offset temp = _r->sp | ( 1 << _r->depth );
	assert( newPid == temp );

	// first, go main page with _r->data as handler
	PageID main_page_id = _r->sp;
	FILE * main_page_handler = _r->data;
	Page main_page = getPage( main_page_handler, _r->sp );
	/**
	 * Attention, is it safe to use this FILE *
	 */
	Store_And_insert_agian( main_page_handler, main_page_id, _r ); 

	// second, use for loop to go through overflow page if exists
	FILE * ov_page_handler = _r->ovflow;
	PageID curr_ov_pid = pageOvflow( main_page );
	free(main_page);

	for( ; curr_ov_pid != NO_PAGE ; ){
		Store_And_insert_agian( ov_page_handler, curr_ov_pid, _r );
		Page next_ov_page = getPage( ov_page_handler, curr_ov_pid );
		curr_ov_pid = pageOvflow( next_ov_page );
		free( next_ov_page ); 
	}

	// after split, reset sp
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
		PageID newp = addNewoverflowPage(r->ovflow, r);
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
		PageID newp = addNewoverflowPage(r->ovflow, r);
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
		PageID newp = addNewoverflowPage(r->ovflow, r);
		// set this page as overflow page of existing primary page(pg)
		pageSetOvflow(pg,newp);
		// this putPage() is basically writing only one new info which is page->ovflow
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
				// get next overflow pageID
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
		PageID newp = addNewoverflowPage(r->ovflow, r);
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

/**
 * For Reln->first_empty_page;
 * struct is 
 * ( Reln->first_empty_page, 0, 1012, x) ->  ( x, 0, 1012, y) -> ( y, 0, 1012, z)
 * 
 * This function will return the tail empty page in order to avoid changing page->ovflow
 */
PageID Tail_empty_page( Reln _r )
{
	PageID temp_ov_pid = _r->first_empty_page;
	if( temp_ov_pid == NO_PAGE ) {
		return NO_PAGE;
	}
	for( ; temp_ov_pid != NO_PAGE ; ) {
		Page curr_ov_page = getPageCertainInfo( _r->ovflow, temp_ov_pid );
		// check the next page id
		if( pageOvflow( curr_ov_page ) != NO_PAGE ) {
			temp_ov_pid = pageOvflow( curr_ov_page );
		}
		// else it is the tail page
		else
		{
			/**
			 * Attention : assert can be deleted
			 */
			assert( pageNTuples(Page) == 0 );
			assert( pageFreeSpace(Page) == 0 );
			free( curr_ov_page );
			break;
		}
		free( curr_ov_page );
	}

	return temp_ov_pid;
}

void Remove_Empty_pid(  Reln _r, PageID _which_one )
{
	PageID temp_ov_pid = _r->first_empty_page;
	if( temp_ov_pid == _which_one ) {
		_r->first_empty_page = NO_PAGE;
		/**
		 * Explain : if there is only one empty overflow page, then the global info(_r->first_empty_page) 
		 * is the one that need to be modified.
		 */
		return;
	}
	for( ; temp_ov_pid != NO_PAGE ; ) {
		Page curr_ov_page = getPageCertainInfo( _r->ovflow, temp_ov_pid );
		// check the next page id
		if( pageOvflow( curr_ov_page ) != _which_one ) {
			temp_ov_pid = pageOvflow( curr_ov_page );
		}
		else{
			UnlinkTailEmptyPage( curr_ov_page );
			putPage( _r->ovflow, temp_ov_pid, curr_ov_page );
			free( curr_ov_page );
			break;
		}
		free( curr_ov_page );
	}
}