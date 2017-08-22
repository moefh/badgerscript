/* mem_pool.c */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "mem_pool.h"

#define INITIAL_PAGE_SIZE (16*1024)

#define ALIGNMENT_SIZE   (sizeof(double))
#define ALIGN(p)         (((size_t)(p) + ALIGNMENT_SIZE-1) & ~(ALIGNMENT_SIZE-1))

struct fh_mem_page {
  struct fh_mem_page *next;
  char *free;
  size_t free_size;
};

void fh_init_mem_pool(struct fh_mem_pool *p)
{
  p->page_list = NULL;
  p->page_size = INITIAL_PAGE_SIZE/2;
}

void fh_destroy_mem_pool(struct fh_mem_pool *p)
{
  struct fh_mem_page *page = p->page_list;
  while (page != NULL) {
    struct fh_mem_page *next = page->next;
    free(page);
    page = next;
  }
}

void *fh_malloc(struct fh_mem_pool *p, size_t size)
{
  if (! p)
    return malloc(size);

  if (size & (ALIGNMENT_SIZE-1))
    size = ALIGN(size);

  struct fh_mem_page *page;
 again:
  page = p->page_list;
  //printf("using page %p, page->free=%p\n", (void *) page, (page) ? (void *)page->free : NULL);
  if (page && page->free_size >= size) {
    void *ret = page->free;
    page->free += size;
    page->free_size -= size;
    //printf("-> fh_malloc(): allocated %zu bytes at %p (page->free=%p)\n", size, ret, (void*)page->free);
    return ret;
  }

  do {
    p->page_size *= 2;
  } while (p->page_size < size + ALIGN(sizeof(struct fh_mem_page)));

  //printf("-> fh_malloc(): [NEW PAGE] allocating %zu bytes for page\n", p->page_size);
  page = malloc(p->page_size);
  if (! page)
    return NULL;

  page->free = (char *)page + ALIGN(sizeof(struct fh_mem_page));

  //printf("size=%zu, align_size=%zu\n", sizeof(struct fh_mem_page), ALIGN(sizeof(struct fh_mem_page)));
  
  page->free_size = p->page_size - ALIGN(sizeof(struct fh_mem_page));
  page->next = p->page_list;
  p->page_list = page;
  goto again;
}

void *fh_realloc(struct fh_mem_pool *p, void *old_data, size_t size)
{
  if (! p)
    return realloc(old_data, size);

  if (size == 0)
    return NULL;
  
  void *new_data = fh_malloc(p, size);
  if (new_data && old_data)
    memcpy(new_data, old_data, size);
  return new_data;
}
