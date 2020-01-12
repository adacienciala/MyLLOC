#ifndef _MYLLOC_H
#define _MYLLOC_H

#include <stdint.h>

#include "custom_unistd.h"

enum pointer_type_t
{
    pointer_null,
    pointer_out_of_heap,
    pointer_control_block,
    pointer_inside_data_block,
    pointer_unallocated,
    pointer_valid
};

struct fence_t
{
    int open[16];
    int close[16];
} fences;

struct memblock_t
{
    struct memblock_t *prev_block;
    struct memblock_t *next_block;
    int size;
    int taken;
    //debug
    int fileline;
    char filename[20];
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

void* heap_malloc_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_debug(void* memblock, size_t size, int fileline, const char* filename);

/// T O - DO ///
void* heap_malloc_aligned(size_t count);
void* heap_calloc_aligned(size_t number, size_t size);
void* heap_realloc_aligned(void* memblock, size_t size);

void* heap_malloc_aligned_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_aligned_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_aligned_debug(void* memblock, size_t size, int fileline, const char* filename);
////////////////

size_t heap_get_used_space(void);
size_t heap_get_largest_used_block_size(void);
uint64_t heap_get_used_blocks_count(void);
size_t heap_get_free_space(void);
size_t heap_get_largest_free_area(void);
uint64_t heap_get_free_gaps_count(void);

enum pointer_type_t get_pointer_type(const const void* pointer);
void* heap_get_data_block_start(const void* pointer);
size_t heap_get_block_size(const const void* memblock);

#endif
