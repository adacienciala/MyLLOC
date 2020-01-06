#ifndef _MYLLOC_H
#define _MYLLOC_H

extern struct memory_list_t the_Heap;

void print_heap();

int heap_setup(void);

void* heap_malloc(size_t count);
void* heap_calloc(size_t number, size_t size);
void heap_free(void* memblock);
void* heap_realloc(void* memblock, size_t size);

void* heap_malloc_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_debug(void* memblock, size_t size, int fileline, const char* filename);


#endif
