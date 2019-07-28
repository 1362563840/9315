// page.c ... functions on Pages
// part of Multi-attribute Linear-hashed Files
// Reading/writing pages into buffers and manipulating contents
// Last modified by John Shepherd, July 2019

#include "defs.h"
#include "page.h"

// internal representation of pages
struct PageRep {
	/**
	 * free = 0 means that the initial address of first free byte is "0 + p->data"
	 */
	Offset free;   // offset within data[] of free space
	Offset ovflow; // Offset of overflow page (if any)
	Count ntuples; // #tuples in this page
	char data[1];  // start of data
};

// A Page is a chunk of memory containing PAGESIZE bytes
// It is implemented as a struct (free, ovflow, data[1])
// - free is the offset of the first byte of free space
// - ovflow is the page id of the next overflow page in bucket
// - data[] is a sequence of bytes containing tuples
// - each tuple is a sequence of chars terminated by '\0'
// - PageID values count # pages from start of file

// create a new initially empty page in memory
Page newPage()
{
	Page p = malloc(PAGESIZE);
	assert(p != NULL);
	p->free = 0;
	p->ovflow = NO_PAGE;
	p->ntuples = 0;
	Count hdr_size = 2*sizeof(Offset) + sizeof(Count);
	int dataSize = PAGESIZE - hdr_size;
	/**
	 * because, p->data is the fourth member of struct PageRep
	 * when using "p->data", it means the address of "p->data"
	 * That is why "dataSize = PAGESIZE - hdr_size;"
	 */
	memset(p->data, 0, dataSize);
	return p;
}

/**
 * This function only add a new main page
 */
// append a new Page to a file; return its PageID
PageID addPage(FILE *f)
{
	int ok = fseek(f, 0, SEEK_END);
	assert(ok == 0);
	int pos = ftell(f);
	assert(pos >= 0);
	PageID pid = pos/PAGESIZE;
	Page p = newPage();
	ok = putPage(f, pid, p);
	assert(ok == 0);
	return pid;
}

// append a new overflow Page to a file; return its PageID
PageID addNewoverflowPage(FILE *_f, Reln _r)
{
	/**
	 * Attention : assert can be deleted
	 */
	assert( ovflowFile( _r ) == _f );
	// Before put a new Page, check if
	PageID temp_pid = Tail_empty_page( _r );
	/**
	 * Attention : check can be deleted
	 */
	// ------------------------------------------------- do some check
	if( temp_pid != NO_PAGE ) {
		Page check_page = getPageCertainInfo( dataFile( _r ), temp_pid );
		assert( pageOvflow( check_page ) == NO_PAGE );
		free( check_page );
	}
	// ------------------------------------------------- do some check
	if( temp_pid != NO_PAGE	) {
		// then there is one empty page
		// use it, and remove it from empty page list
		Remove_Empty_pid( _r, temp_pid );
		return temp_pid;
	}

	// no empty page
	int ok = fseek(_f, 0, SEEK_END);
	assert(ok == 0);
	int pos = ftell(_f);
	assert(pos >= 0);
	PageID pid = pos/PAGESIZE;
	Page p = newPage();
	ok = putPage(_f, pid, p);
	assert(ok == 0);
	return pid;
}

// fetch a Page from a file; allocate a memory buffer
Page getPage(FILE *f, PageID pid)
{
	assert(pid >= 0);
	Page p = malloc(PAGESIZE);
	assert(p != NULL);
	int ok = fseek(f, pid*PAGESIZE, SEEK_SET);
	assert(ok == 0);
	int n = fread(p, 1, PAGESIZE, f);
	assert(n == PAGESIZE);
	return p;
}

/***
 * Warning, this page contains part of global info, not even cv, let alone tuples
 */
Page getPageCertainInfo(FILE *f, PageID pid)
{
	assert(pid >= 0);
	/**
	 * Becuase PageId, Count, Offset are same size
	 */
	Page p = malloc( sizeof( Offset ) * 3 );
	assert(p != NULL);
	// move to target page
	int ok = fseek(f, pid*PAGESIZE, SEEK_SET);
	assert(ok == 0);
	int n = fread(p, 1, ( sizeof( Offset ) * 3 ), f);
	assert( n == sizeof( Offset ) * 3 );
	return p;
}

// write a Page to a file; release allocated buffer
Status putPage(FILE *f, PageID pid, Page p)
{
	assert(pid >= 0);
	int ok = fseek(f, pid*PAGESIZE, SEEK_SET);
	assert(ok == 0);
	int n = fwrite(p, 1, PAGESIZE, f);
	assert(n == PAGESIZE);
	free(p);
	return 0;
}

// insert a tuple into a page
// returns 0 status if successful
// returns -1 if not enough room
Status addToPage(Page p, Tuple t)
{
	int n = tupLength(t);
	char *c = p->data + p->free;
	Count hdr_size = 2*sizeof(Offset) + sizeof(Count);
	// doesn't fit ... return fail code
	// assume caller will put it elsewhere
	if (c+n > &p->data[PAGESIZE-hdr_size-2]) return -1;
	strcpy(c, t);
	p->free += n+1;
	p->ntuples++;
	return OK;
}

/**
 * TODO
 * 
 * 
 * Difference between split verions and non split verion is that
 * if this tuple still belongs to old page, then do nothing
 */
Status addToPageSplitVersion(Page p, Tuple t)
{
	int n = tupLength(t);
	char *c = p->data + p->free;
	Count hdr_size = 2*sizeof(Offset) + sizeof(Count);
	// doesn't fit ... return fail code
	// assume caller will put it elsewhere
	if (c+n > &p->data[PAGESIZE-hdr_size-2]) return -1;
	strcpy(c, t);
	p->free += n+1;
	p->ntuples++;
	return OK;
}


// extract page info
char *pageData(Page p) { return p->data; }
Count pageNTuples(Page p) { return p->ntuples; }
Offset pageOvflow(Page p) { return p->ovflow; }
void pageSetOvflow(Page p, PageID pid) { p->ovflow = pid; }
Count pageFreeSpace(Page p) {
	Count hdr_size = 2*sizeof(Offset) + sizeof(Count);
	return (PAGESIZE-hdr_size-p->free);
}


void resetPageInfo( FILE *_handler, PageID _pid, Page _which_page )
{
	_which_page->free = 0;
	// which_page->ovflow = which_page->ovflow;
	_which_page->ntuples = 0;
	memset( &_which_page->data[0], 0, ( PAGESIZE- 2*sizeof(Offset) - sizeof(Count) ) );
	putPage( _handler, _pid, _which_page );
}

/**
 * parse the page just before the tail page, set the ovflow as NO_PAGE
 */
void UnlinkTailEmptyPage(Page _page_before_tail_page)
{
	/**
	 * Attention, aseert can be deleted
	 */
	assert( _page_before_tail_page->free == 0 );
	assert( _page_before_tail_page->ntuples == 0 );
	_page_before_tail_page->ovflow = NO_PAGE;
}

/**
 * It does not actually delete, just free this page from list
 * 
 * Attention, _handler should be r->ovflow
 * 
 * Remember , you still need to add this page to r->first_empty_page
 */
void deleteNode( FILE * _handler, PageID _faterPID, PageID _deletedPID )
{
	Page fatherPage = getPage( _handler, _faterPID );
	Page sonPage = getPage( _handler, _deletedPID );
	
	// son does not have grandson;
	if( sonPage->ovflow == NO_PAGE ) {
		fatherPage->ovflow = NO_PAGE;
	}
	// son does have
	else{
		fatherPage->ovflow = sonPage->ovflow;
	}
	sonPage->ovflow = NO_PAGE;

	putPage( _handler, _faterPID, fatherPage );
	putPage( _handler, _deletedPID, sonPage );
	
	/**
	 * Because putPage() above use free();
	 */
	// free(fatherPage);
	// free(sonPage);
}

/**
 * Attention, _handler should be r->ovflow
 */
void InsertOvEmptyPid( FILE * _handler, PageID _last_pageID, PageID _goingToBeAdded_pid )
{
	Page fatherPage = getPage( _handler, _last_pageID );
	/**
	 * Attention, this assert can be deleted
	 */
	assert( fatherPage->ovflow == NO_PAGE );
	fatherPage->ovflow = _last_pageID;
	putPage( _handler, _last_pageID, fatherPage );

	/**
	 * Because putPage() above use free();
	 */
	// free(fatherPage);
}	