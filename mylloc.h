#ifndef _MYLLOC_H
#define _MYLLOC_H

#include "custom_unistd.h"

struct memblock_t
{
    struct memblock_t *prev_block;
    struct memblock_t *next_block;
    int size;
    int taken;
};

struct memory_list_t
{
    intptr_t start_brk;
    intptr_t brk;
} the_Heap;

void print_heap();

int heap_setup(void);

void* heap_malloc(size_t count);
void* heap_calloc(size_t number, size_t size);
void heap_free(void* memblock);
void* heap_realloc(void* memblock, size_t size);


#endif
