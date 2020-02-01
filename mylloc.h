#ifndef _MYLLOC_H
#define _MYLLOC_H

#include <stdint.h>
#include <pthread.h>

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

pthread_mutex_t heap_mutex;
pthread_mutexattr_t attr;

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
    int crc;
    int fileline;
    char* filename;
};

struct memory_list_t
{
    intptr_t start_brk;
    intptr_t brk;
    size_t heap_used_space;
    size_t heap_largest_used;
    uint64_t heap_used_blocks;
    size_t heap_free_space;
    size_t heap_largest_free;
    uint64_t heap_free_blocks;
    int heap_crc;
} the_Heap;

int heap_setup(void);

void* heap_malloc(size_t count);
void* heap_calloc(size_t number, size_t size);
void heap_free(void* memblock);
void* heap_realloc(void* memblock, size_t size);

void* heap_malloc_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_debug(void* memblock, size_t size, int fileline, const char* filename);

void* heap_malloc_aligned(size_t count);
void* heap_calloc_aligned(size_t number, size_t size);
void* heap_realloc_aligned(void* memblock, size_t size);

void* heap_malloc_aligned_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_aligned_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_aligned_debug(void* memblock, size_t size, int fileline, const char* filename);

size_t heap_get_used_space(void);
size_t heap_get_largest_used_block_size(void);
uint64_t heap_get_used_blocks_count(void);
size_t heap_get_free_space(void);
size_t heap_get_largest_free_area(void);
uint64_t heap_get_free_gaps_count(void);

enum pointer_type_t get_pointer_type(const const void* pointer);
void* heap_get_data_block_start(const void* pointer);
size_t heap_get_block_size(const const void* memblock);

void heap_dump_debug_information(void);
int heap_validate(void);

void update_crc (struct memblock_t *p);
void update_heap();
void heap_restart();

#endif
