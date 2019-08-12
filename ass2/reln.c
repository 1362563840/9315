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
#include <math.h>

#define HEADERSIZE (3*sizeof(Count)+sizeof(Offset))

void freeBackup( char **backup, int how_many_tuples );
void BackTuple( char **backup, int how_many_existing_tuples, char *start, char *end );
void StoreEmptyOvPage( Reln _r, PageID _empty_Page_pid );
void Display( Reln _r );
void SplitPage( Reln _r );
void collectEmptyPage( Reln _r );
void Store_And_insert_agian( FILE *_handler, PageID _pid, Reln _r );

int int_pow(int base, int exp)
{
    int result = 1;
    while (exp)
    {
        if (exp & 1)
           result *= base;
        exp /= 2;
        base *= base;
    }
    return result;
}

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


/**
 * after splitting is done, need to check if there is any empty overflow page , and collect them
 * 
 * The only place empty page in list can exist is the list starting with the page pointed by _r->sp
 * 
 * Remember to putPage()
 */
void collectEmptyPage( Reln _r )
{
	Page main_page = getPage( _r->data, _r->sp );
	
	PageID father_PID = _r->sp;
	PageID curr_ov_pageID = pageOvflow( main_page );
	free(main_page);
	// before go to next loop, if the page after main page is empty ov page, then need to adjust file handler

	/**
	 * before go to next loop, if the father_page is stll main page, then need to adjust file handler
	 */
	for( ; curr_ov_pageID != NO_PAGE ; ) {
		Page curr_ov_page = getPage( _r->ovflow, curr_ov_pageID );
		if( pageNTuples( curr_ov_page ) == 0 ) {
			PageID temp_son_node_of_deleted_node = pageOvflow( curr_ov_page );
			/**
			 * Attention, debug
			 */
			checkPgeAssert( curr_ov_page );

			// free( curr_ov_page );
			deleteNodeFatherIsMain( _r->data, _r->ovflow, father_PID, curr_ov_pageID );

			StoreEmptyOvPage( _r, curr_ov_pageID );

			free( curr_ov_page );
			curr_ov_pageID = temp_son_node_of_deleted_node;
			continue;
		}

		father_PID = curr_ov_pageID;
		curr_ov_pageID = pageOvflow( curr_ov_page );
		free( curr_ov_page );

		break;
	}

	// previous loop ends through " curr_ov_pageID != NO_PAGE", then the next loop will not go
	// unless it is through break;

	for( ; curr_ov_pageID != NO_PAGE ; ) {	
		Page curr_ov_page = getPage( _r->ovflow, curr_ov_pageID );
		// check if empty by checking #numOftuple
		if( pageNTuples( curr_ov_page ) == 0 ) {
			/**
			 * The reason that do the three lines below before deleteNode() and StoreEmptyOvPage()
			 * is that when a node is going to be deleted, the son node info need to be recorded, 
			 * otherwise the list is corrupted.
			 */
			father_PID = father_PID; // this line actually does nothing
			PageID temp_son_node_of_deleted_node = pageOvflow( curr_ov_page );
			free( curr_ov_page );

			deleteNode( _r->ovflow, father_PID, curr_ov_pageID );
			// after delted, need to store this empty ov page info to _r->first_empty_page
			StoreEmptyOvPage( _r, curr_ov_pageID );
			
			// after deleteNode() and StoreEmptyOvPage() are finsihed, need to adjust variable "curr_ov_pageID",
			// let it point to the son of deleted node. Son node can be NO_PAGE, it is fine
			curr_ov_pageID = temp_son_node_of_deleted_node;

			// can not use break; mayber there are other empty ov pages;
			continue;
		}

		father_PID = curr_ov_pageID;
		curr_ov_pageID = pageOvflow( curr_ov_page );
		free( curr_ov_page );
	}

}

void Store_And_insert_agian( FILE *_handler, PageID _pid, Reln _r ) 
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
	char **backup = calloc( how_many_tuples_curr_page, sizeof( char * ) );
	assert( backup != NULL );
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

		// end of a tuple					//		&& end != NULl // Attention
		if( *(initial + i) == '\0' && start != NULL ) {
			end = initial + i;
			// store this tuple	into backup
			BackTuple( backup, existing_scanned_tuples_num, start, end );
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
			assert( end == NULL );
			break;
		}

	}
	// Attention
	assert( existing_scanned_tuples_num == how_many_tuples_curr_page );
	// after store
	// reset this page's 3 members, the last one data[1] should be same
	// reset the tuple parts
	_r->ntups = _r->ntups - how_many_tuples_curr_page;
	resetPageInfo( _handler, _pid, curr_page );

	// insert again, except this time you use one more bit of hash value
	for( int i = 0 ; i < how_many_tuples_curr_page ; i++ ) {
		addToRelationSplitVersion( _r, backup[ i ] );
	}
	// free char **backup, curr_page
	freeBackup( backup, existing_scanned_tuples_num );

	// in resetPageInfo(), it calls putPage(), putPage() will call free()
	// free( curr_page );
	return;
}

/**
 * According to forum 
 * We first read all tuples and store it in in-memory
 * Then reset page pointed by sp
 * Then insert again
 */
void SplitPage( Reln _r )
{	
	// create a new main page
	PageID newPid = addPage( _r->data );	
	_r->npages++;

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
	
	// before reset sp, collect empty ov pages if exists
	collectEmptyPage( _r );

	// after split, reset sp, depth
	if( _r->sp == ( int_pow( 2, _r->depth ) - 1 ) ){
		_r->sp = 0;
		_r->depth = _r->depth + 1;
	}
	else{
		_r->sp = _r->sp + 1;
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

PageID addToRelation(Reln r, Tuple t)
{
	int C = 1024/(10*r->nattrs) ;
	if( r->ntups % C == 0 && r->ntups != 0 ) {
		SplitPage( r );
	}

	Bits h, p;
	h = tupleHash(r,t);
	if (r->depth == 0)
		p = 1;
	else {
		p = getLower(h, r->depth);
		if (p < r->sp) p = getLower(h, r->depth+1);
	}

	Page pg = getPage(r->data,p);
	if (addToPage(pg,t) == OK) {
		/**
		 * Attention, debug
		 */
		checkPgeAssert( pg );

		putPage(r->data,p,pg);
		r->ntups++;
		return p;
	}

	// primary data page full
	// no overflow page
	if (pageOvflow(pg) == NO_PAGE) {
		// add first overflow page in chain

		// create a new overflow page 
		PageID newp = addNewoverflowPage(r->ovflow, r);
		// set this page as overflow page of existing primary page(pg)
		pageSetOvflow(pg,newp);

		/**
		 * Attention, debug
		 */
		checkPgeAssert( pg );

		putPage(r->data,p,pg);
		Page newpg = getPage(r->ovflow,newp);
		// can't add to a new overflow page; we have a problem
		if (addToPage(newpg,t) != OK){
			free( newpg );
			return NO_PAGE;
		}

		/**
		 * Attention, debug
		 */
		checkPgeAssert( newpg );

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
				prevp = ovp;
				// before assigning "prevpg" current page, need to free already pointing
				if( prevpg != NULL ) {
					free(prevpg);
				}
				prevpg = ovpg;
				ovp = pageOvflow(ovpg);
			}
			else {
				if (prevpg != NULL) free(prevpg);

				// putPage() help us free "ovpg"
				/**
				 * Attention, debug
				 */
				checkPgeAssert( ovpg );

				putPage(r->ovflow,ovp,ovpg);
				r->ntups++;
				free( pg );
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

		/**
		 * Attention, debug
		 */
		checkPgeAssert( newpg );
		
        putPage(r->ovflow,newp,newpg);
		// link to existing overflow chain
		pageSetOvflow(prevpg,newp);

		/**
		 * Attention, debug
		 */
		checkPgeAssert( prevpg );
		
		putPage(r->ovflow,prevp,prevpg);
        r->ntups++;
		free( pg );
		return p;
	}
	
	return NO_PAGE;
}

PageID addToRelationSplitVersion(Reln r, Tuple t)
{
	Bits h, p;
	h = tupleHash(r,t);
	p = getLower(h, r->depth+1);


	Page pg = getPage(r->data,p);
	if (addToPage(pg,t) == OK) {

		/**
		 * Attention, debug
		 */
		checkPgeAssert( pg );
		
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
		
		/**
		 * Attention, debug
		 */
		checkPgeAssert( pg );

		putPage(r->data,p,pg);
		Page newpg = getPage(r->ovflow,newp);
		// can't add to a new overflow page; we have a problem
		if (addToPage(newpg,t) != OK) {
			free( newpg );
			return NO_PAGE;
		} 

		/**
		 * Attention, debug
		 */
		checkPgeAssert( newpg );
		
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
				prevp = ovp;
				if( prevpg != NULL ) {
					free(prevpg);
				} 
				prevpg = ovpg;
				// get next overflow pageID
				ovp = pageOvflow(ovpg);
			}
			else {
				if (prevpg != NULL) free(prevpg);

				/**
				 * Attention, debug
				 */
				checkPgeAssert( ovpg );
				
				putPage(r->ovflow,ovp,ovpg);
				r->ntups++;
				free( pg) ;
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

		/**
		 * Attention, debug
		 */
		checkPgeAssert( newpg );
		
        putPage(r->ovflow,newp,newpg);
		// link to existing overflow chain
		pageSetOvflow(prevpg,newp);

		/**
		 * Attention, debug
		 */
		checkPgeAssert( prevpg );
		
		putPage(r->ovflow,prevp,prevpg);
        r->ntups++;
		free( pg) ;
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
			assert( pageNTuples(curr_ov_page) == 0 );
			assert( pageFreeSpace(curr_ov_page) == 1012 );

			free( curr_ov_page );
			break;
		}
		free( curr_ov_page );
	}

	return temp_ov_pid;
}

void Remove_Empty_pid( Reln _r, PageID _which_one )
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
	// do not know if remove successful, just assume?
	for( ; temp_ov_pid != NO_PAGE ; ) {
		Page curr_ov_page = getPage( _r->ovflow, temp_ov_pid );
		// check the next page id
		if( pageOvflow( curr_ov_page ) != _which_one ) {
			temp_ov_pid = pageOvflow( curr_ov_page );
		}
		else{
			UnlinkTailEmptyPage( curr_ov_page );

			/**
			 * Attention, debug
			 */
			checkPgeAssert( curr_ov_page );
			
			putPage( _r->ovflow, temp_ov_pid, curr_ov_page );
			// because putPage() already free(), so no need to free again
			// free( curr_ov_page );
			break;
		}
		free( curr_ov_page );
	}
	
	return;
}

void StoreEmptyOvPage( Reln _r, PageID _empty_Page_pid )
{
	if( _r->first_empty_page == NO_PAGE ) {
		// This is global info, so do not need to putpage()
		_r->first_empty_page = _empty_Page_pid;
		return;
	}
	// find the tail ov page in the list which stores all empty ov pages
	PageID curr_pageID = _r->first_empty_page;
	for(  ; curr_pageID != NO_PAGE ; ) {
		Page curr_page = getPage( _r->ovflow, curr_pageID );
		// check if curr_page has child node, if not, this is the tail page
		if( pageOvflow( curr_page ) == NO_PAGE ) {
			// link new empty ov page to "curr_page", curr_page must be in file overflow
			// need to putpage(), linkNewFreeOvPage() does putpage()

			/**
			 * Attention, debug
			 */
			checkPgeAssert( curr_page );
			
			linkNewFreeOvPage( _r->ovflow, curr_pageID, curr_page, _empty_Page_pid);
			// because linkNewFreeOvPage() has putpage(), so it does free() already
			// free(curr_page);
			break;
		}
		curr_pageID = pageOvflow( curr_page );
		free(curr_page);
	}
	
	/**
	 * Attention, this assert can be deleted
	 */
	Page check_page = getPage( _r->ovflow, curr_pageID ); // page before last one
	assert( pageOvflow(check_page) == _empty_Page_pid );
	free(check_page);

}

void freeBackup( char **backup, int how_many_tuples ) {
	for( int i = 0; i < how_many_tuples ; i++ ) {
		free( backup[ i ] );
	}
	free( backup );
}

/**
 * 															including	including
 * 																	*end == '\0'
 */
void BackTuple( char **backup, int how_many_existing_tuples, char *start, char *end )
{											//
	char *temp = calloc( sizeof( char ) * ( end - start + 1 ), 1 );
	assert( temp != NULL );
	char *offset = start;
	for( int i = 0 ; offset <= end ; i++ ) {
		temp[ i ] = *offset;
		offset = offset + 1;
	}
	backup[ how_many_existing_tuples ] = temp;
}

Count ReadTupleFromPage( Reln _r, char *_Data_part ) 
{
	const Count hdr_size = 2*sizeof(Offset) + sizeof(Count); // make it const
	Count existing_scanned_tuples_num = 0;
	char * const initial = _Data_part;

	char *start = NULL;
	char *end = NULL;
	char *last_end = NULL;
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
			// PrintOneTuple( _r, start, end );
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

			break;
		}

	}
	return existing_scanned_tuples_num;
}

/**
 * Debug function, delete
 */ 
void Display( Reln _r )
{
	// go through all main pages
	for( int i = 0 ; i < _r->npages ; i++ ) {
		printf( "cur main page id is %d\n", i );
		
		Page curr_main_page = getPage( _r->data, i );
		Count how_many_tuples_curr_page = pageNTuples( curr_main_page );

		char * const initial = pageData( curr_main_page );
		if( pageNTuples( curr_main_page ) != 0 ) {
			Count result = ReadTupleFromPage( _r, initial );
			assert( result == how_many_tuples_curr_page );
		}
		else {
			printf("No tuple at this page\n");
		}

		checkPgeAssert( curr_main_page );
		PageID ovPage = pageOvflow( curr_main_page ) ;
		free(curr_main_page);

		for( ; ovPage != NO_PAGE ;  ) {
			printf( "cur overflow page is %d attached to %d\n", ovPage , i );
			Page temp_ov_page = getPage( _r->ovflow, ovPage );

			Count temp_how_many_tuples_curr_page = pageNTuples( temp_ov_page );
			char * const temp_init = pageData( temp_ov_page );

			if( pageNTuples( temp_ov_page ) != 0 ) {
				Count temp_result = ReadTupleFromPage( _r, temp_init );
				if( temp_how_many_tuples_curr_page != temp_result ) {
					printf("first is %d, second is %d\n", temp_how_many_tuples_curr_page, temp_result);
				}	
				assert( temp_how_many_tuples_curr_page == temp_result );
			}
			else {
				printf("No tuple at this page\n");
			}
			checkPgeAssert( temp_ov_page );
			ovPage = pageOvflow( temp_ov_page );
			free(temp_ov_page);
		}
	}
}

/**
 * Debug function, delete
 */ 
void DisplayOvPageInfo( Reln _r )
{
	if( _r->first_empty_page == NO_PAGE ) {
		printf("There is no free overflow page\n");
	}
	else{
		PageID currPID = _r->first_empty_page;
		printf("ov id is %d ", currPID );
		Page curr_page = getPage( _r->ovflow, currPID );
		currPID = pageOvflow( curr_page );
		free(curr_page);
		for( ; currPID != NO_PAGE ; ) {
			printf(" -> ov id is %d ", currPID );
			Page temp_curr_page = getPage( _r->ovflow, currPID );
			currPID = pageOvflow( temp_curr_page );
			free(temp_curr_page);
		}
	}
	printf("\n");
}