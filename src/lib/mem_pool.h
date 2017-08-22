/* mem_pool.h */

#ifndef MEM_POOL_H_FILE
#define MEM_POOL_H_FILE

struct fh_mem_page;

struct fh_mem_pool {
  size_t page_size;
  struct fh_mem_page *page_list;
};

void fh_init_mem_pool(struct fh_mem_pool *p);
void fh_destroy_mem_pool(struct fh_mem_pool *p);
void *fh_malloc(struct fh_mem_pool *p, size_t size);
void *fh_realloc(struct fh_mem_pool *p, void *data, size_t size);

#define fh_free(p, data) fh_realloc((p), (data), 0)

#endif /* MEM_POOL_H_FILE */
