// page.h ... interface to functions on Pages
// part of Multi-attribute Linear-hashed Files
// See pages.c for descriptions of Page type and functions
// Last modified by John Shepherd, July 2019

#ifndef PAGE_H
#define PAGE_H 1

typedef struct PageRep *Page;

#include "defs.h"
#include "tuple.h"

Page newPage();
PageID addPage(FILE *);
Page getPage(FILE *, PageID);
Page getPageCertainInfo(FILE *f, PageID pid);
Status putPage(FILE *, PageID, Page);
Status addToPage(Page, Tuple);
char *pageData(Page);
Count pageNTuples(Page);
Offset pageOvflow(Page);
void pageSetOvflow(Page, PageID);
Count pageFreeSpace(Page);

void resetPageInfo( FILE *_handler, PageID _pid, Page _which_page );

void UnlinkTailEmptyPage(Page which_page);
PageID addNewoverflowPage(FILE *_f, Reln _r);

#endif
