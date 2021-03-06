#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mylloc.h"

#define WORD_SIZE sizeof(void *)
#define PAGE_SIZE 4096
#define FENCE_SIZE (sizeof(fences.open))
#define MEMBLOCK_SIZE (sizeof(struct memblock_t))
#define CUR_HEAP_SIZE (the_Heap.brk - the_Heap.start_brk)
#define SET_FENCES(ptr, count) memcpy((void *)((intptr_t)(ptr) + MEMBLOCK_SIZE), fences.open, FENCE_SIZE); \
memcpy((void *)((intptr_t)(ptr) + MEMBLOCK_SIZE + FENCE_SIZE + count), fences.close, FENCE_SIZE)

int heap_setup(void)
{
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&heap_mutex, &attr);

    pthread_mutex_lock(&heap_mutex);
    the_Heap.start_brk = (intptr_t) custom_sbrk(MEMBLOCK_SIZE * 2);
	if ((void *)the_Heap.start_brk == NULL)
    {
        pthread_mutex_unlock(&heap_mutex);
        return -1;
    }

	the_Heap.brk = the_Heap.start_brk + MEMBLOCK_SIZE;

    ((struct memblock_t *)(the_Heap.start_brk))->next_block = (struct memblock_t *)(the_Heap.brk);
    ((struct memblock_t *)(the_Heap.start_brk))->prev_block = NULL;
    ((struct memblock_t *)(the_Heap.start_brk))->size = 0;
    ((struct memblock_t *)(the_Heap.start_brk))->taken = 1;
    update_crc((struct memblock_t *)the_Heap.start_brk);

    ((struct memblock_t *)the_Heap.brk)->next_block = NULL;
    ((struct memblock_t *)the_Heap.brk)->prev_block = (struct memblock_t *)the_Heap.start_brk;
    ((struct memblock_t *)the_Heap.brk)->size = 0;
    ((struct memblock_t *)the_Heap.brk)->taken = 1;
    update_crc((struct memblock_t *)the_Heap.brk);

    the_Heap.brk = the_Heap.brk + MEMBLOCK_SIZE;

    for (int i=0; i<(FENCE_SIZE/sizeof(fences.open[0])); ++i)
    {
        fences.open[i] = rand();
        fences.close[i] = rand();
    }

    the_Heap.heap_used_space = heap_get_used_space();
    the_Heap.heap_largest_used = heap_get_largest_used_block_size();
    the_Heap.heap_used_blocks = heap_get_used_blocks_count();
    the_Heap.heap_free_space = heap_get_free_space();
    the_Heap.heap_largest_free = heap_get_largest_free_area();
    the_Heap.heap_free_blocks = heap_get_free_gaps_count();
    update_heap();

    pthread_mutex_unlock(&heap_mutex);
    return heap_validate();
}

void* heap_malloc(size_t count)
{
    return heap_malloc_debug(count, 0, NULL);
}

void* heap_calloc(size_t number, size_t size)
{
    return heap_calloc_debug(number, size, 0, NULL);
}

void heap_free(void* memblock)
{
    pthread_mutex_lock(&heap_mutex);
    if (get_pointer_type(memblock) == pointer_valid && heap_validate() == 0)
    {
        struct memblock_t *p = (struct memblock_t *)((intptr_t )memblock - FENCE_SIZE - MEMBLOCK_SIZE);
        p->taken = 0;

        // coalesce eventual free neighbours
        if (p->prev_block->taken == 0)
        {
            p->prev_block->next_block = p->next_block;
            p->next_block->prev_block = p->prev_block;
            p = p->prev_block;
        }
        if (p->next_block->taken == 0)
        {
            p->next_block = p->next_block->next_block;
            p->next_block->prev_block = p;
        }

        p->size = (int)((intptr_t)(p->next_block) - (intptr_t)p - MEMBLOCK_SIZE);
        update_crc(p->next_block);
        update_crc(p->prev_block);
        update_crc(p);
    }
    pthread_mutex_unlock(&heap_mutex);
}

void* heap_realloc(void* memblock, size_t size)
{
    return heap_realloc_debug(memblock, size, 0, NULL);
}

////////////////////////////////////////
////// ------- D E B U G ------- ///////
////////////////////////////////////////

void* heap_malloc_debug(size_t count, int fileline, const char* filename)
{
    pthread_mutex_lock(&heap_mutex);

    if (count == 0 || heap_validate())
    {
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }

    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    int found = 0;
    while (p != NULL)
    {
        if ((p->size == (count + FENCE_SIZE*2) || p->size >= (count + FENCE_SIZE*2 + MEMBLOCK_SIZE)) && p->taken == 0)
        {
            found = 1;
            break;
        }
        p = p->next_block;
    }

    if (found == 0)
    {
        p = (struct memblock_t *)(the_Heap.brk - MEMBLOCK_SIZE);
        the_Heap.brk = (intptr_t) custom_sbrk(MEMBLOCK_SIZE + FENCE_SIZE * 2 + count);
        if ((void *)the_Heap.brk == NULL)
        {
            pthread_mutex_unlock(&heap_mutex);
            return NULL;
        }

        the_Heap.brk = the_Heap.brk + count + FENCE_SIZE * 2;
        ((struct memblock_t *)the_Heap.brk)->size = 0;
        ((struct memblock_t *)the_Heap.brk)->taken = 1;
        ((struct memblock_t *)the_Heap.brk)->next_block = NULL;
        ((struct memblock_t *)the_Heap.brk)->prev_block = p;
        ((struct memblock_t *)the_Heap.brk)->fileline = fileline;
        ((struct memblock_t *)the_Heap.brk)->filename = (char *)filename;
        update_crc(((struct memblock_t *)the_Heap.brk)->prev_block);
        update_crc((struct memblock_t *)the_Heap.brk);

        p->size = count;
        p->taken = 1;
        p->next_block = (struct memblock_t *)the_Heap.brk;
        p->fileline = fileline;
        p->filename = (char *)filename;
        update_crc(p->prev_block);
        update_crc(p->next_block);
        update_crc(p);
        SET_FENCES(p, count);
        the_Heap.brk = (the_Heap.brk) + MEMBLOCK_SIZE;

        if (heap_validate())
        {
            pthread_mutex_unlock(&heap_mutex);
            return NULL;
        }
        else
        {
            pthread_mutex_unlock(&heap_mutex);
            return (void *)((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE);
        }
    }

    // add a new memblock, if enough space
    if (p->size >= (int)(count + MEMBLOCK_SIZE + FENCE_SIZE*2))
    {
        intptr_t temp = (intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE*2 + count;
        ((struct memblock_t *)temp)->next_block = p->next_block;
        p->next_block->prev_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->prev_block = p;
        p->next_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->taken = 0;
        ((struct memblock_t *)temp)->size = p->size - (int)(MEMBLOCK_SIZE + FENCE_SIZE*2 + count);
        ((struct memblock_t *)temp)->fileline = fileline;
        ((struct memblock_t *)temp)->filename = (char *)filename;
        update_crc(((struct memblock_t *)temp)->next_block);
        update_crc(((struct memblock_t *)temp)->prev_block);
        update_crc((struct memblock_t *)temp);
    }

    p->size = (int)((intptr_t)p->next_block - ((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE*2));
    SET_FENCES(p, p->size);
    p->taken = 1;
    p->fileline = fileline;
    p->filename = (char *)filename;
    update_crc(p->prev_block);
    update_crc(p->next_block);
    update_crc(p);

    if (heap_validate())
    {
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }
    else
    {
        pthread_mutex_unlock(&heap_mutex);
        return (void *)((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE);
    }
}

void* heap_calloc_debug(size_t number, size_t size, int fileline, const char* filename)
{
    pthread_mutex_lock(&heap_mutex);

    if (number == 0 || size == 0 || heap_validate())
    {
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }

    void *p = heap_malloc_debug(number * size, fileline, filename);

    if (p != NULL) memset (p, 0, number*size);

    if (heap_validate())
    {
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }
    else
    {
        pthread_mutex_unlock(&heap_mutex);
        return p;
    }
}

void* heap_realloc_debug(void* memblock, size_t size, int fileline, const char* filename)
{
    pthread_mutex_lock(&heap_mutex);

    if (heap_validate())
    {
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }
    if (memblock == NULL)
    {
        pthread_mutex_unlock(&heap_mutex);
        return heap_malloc_debug(size, fileline, filename);
    }

    if (size == 0)
    {
        heap_free(memblock);
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }

    struct memblock_t *p = (struct memblock_t *)(((intptr_t )memblock - (MEMBLOCK_SIZE + FENCE_SIZE)));
    int size_to_copy = (p->size > size) ? (int)size : p->size;

    heap_free(memblock);
    void *new = heap_malloc_debug(size, fileline, filename);
    if (new != NULL) memcpy(new, memblock, size_to_copy);

    if (heap_validate())
    {
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }
    else
    {
        pthread_mutex_unlock(&heap_mutex);
        return new;
    }
}

////////////////////////////////////////
////// ----- A L I G N E D ----- ///////
////////////////////////////////////////

void* heap_malloc_aligned(size_t count)
{
    return heap_malloc_aligned_debug(count, 0, NULL);
}

void* heap_calloc_aligned(size_t number, size_t size)
{
    return heap_calloc_aligned_debug(number, size, 0, NULL);
}

void* heap_realloc_aligned(void* memblock, size_t size)
{
    return heap_realloc_aligned_debug(memblock, size, 0, NULL);
}


////////////////////////////////////////
////// ----- A L I - D E B ----- ///////
////////////////////////////////////////

void* heap_malloc_aligned_debug(size_t count, int fileline, const char* filename)
{
    pthread_mutex_lock(&heap_mutex);
    if (count == 0 || heap_validate())
    {
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }

    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    int found = 0;
    while (p != NULL)
    {
        if (((p->size == (count + FENCE_SIZE*2) || p->size >= (count + FENCE_SIZE*2 + MEMBLOCK_SIZE)) && p->taken == 0) && ((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE) % PAGE_SIZE == 0)
        {
            found = 1;
            break;
        }
        p = p->next_block;
    }

    if (found == 0)
    {
        p = (struct memblock_t *)(the_Heap.brk - MEMBLOCK_SIZE);
        struct memblock_t *temp = p->prev_block;

        size_t needed_space = (PAGE_SIZE - (CUR_HEAP_SIZE % PAGE_SIZE) - FENCE_SIZE) >= 0 ? (PAGE_SIZE - (CUR_HEAP_SIZE % PAGE_SIZE)): (PAGE_SIZE - (CUR_HEAP_SIZE % PAGE_SIZE) + PAGE_SIZE);
        the_Heap.brk = (intptr_t) custom_sbrk(MEMBLOCK_SIZE + FENCE_SIZE + count + needed_space);
        if ((void *)the_Heap.brk == NULL)
        {
            pthread_mutex_unlock(&heap_mutex);
            return NULL;
        }

        p = (struct memblock_t *)((intptr_t)p + needed_space - FENCE_SIZE);

        the_Heap.brk = the_Heap.brk + count + FENCE_SIZE + needed_space;
        ((struct memblock_t *)the_Heap.brk)->size = 0;
        ((struct memblock_t *)the_Heap.brk)->taken = 1;
        ((struct memblock_t *)the_Heap.brk)->next_block = NULL;
        ((struct memblock_t *)the_Heap.brk)->prev_block = p;
        ((struct memblock_t *)the_Heap.brk)->filename = (char *)filename;
        ((struct memblock_t *)the_Heap.brk)->fileline = fileline;
        update_crc(((struct memblock_t *)the_Heap.brk)->prev_block);
        update_crc((struct memblock_t *)the_Heap.brk);

        p->size = count;
        p->taken = 1;
        p->next_block = (struct memblock_t *)the_Heap.brk;
        p->prev_block = temp;
        temp->next_block = p;
        p->filename = (char *)filename;
        p->fileline = fileline;
        update_crc(p->prev_block);
        update_crc(p->next_block);
        update_crc(p);
        SET_FENCES(p, count);
        the_Heap.brk = the_Heap.brk + MEMBLOCK_SIZE;

        if (heap_validate())
        {
            pthread_mutex_unlock(&heap_mutex);
            return NULL;
        }
        else
        {
            pthread_mutex_unlock(&heap_mutex);
            return (void *)((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE);
        }
    }

    // add a new memblock, if enough space
    if (p->size >= (int)(count + MEMBLOCK_SIZE + FENCE_SIZE*2))
    {
        intptr_t temp = (intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE*2 + count;
        ((struct memblock_t *)temp)->next_block = p->next_block;
        p->next_block->prev_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->prev_block = p;
        p->next_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->taken = 0;
        ((struct memblock_t *)temp)->filename = (char *)filename;
        ((struct memblock_t *)temp)->fileline = fileline;
        ((struct memblock_t *)temp)->size = p->size - (int)(MEMBLOCK_SIZE + FENCE_SIZE*2 + count);
        update_crc(((struct memblock_t *)temp)->next_block);
        update_crc(((struct memblock_t *)temp)->prev_block);
        update_crc((struct memblock_t *)temp);
    }

    p->size = (int)((intptr_t)p->next_block - ((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE*2));
    SET_FENCES(p, p->size);
    p->taken = 1;
    p->filename = (char *)filename;
    p->fileline = fileline;
    update_crc(p->prev_block);
    update_crc(p->next_block);
    update_crc(p);

    if (heap_validate())
    {
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }
    else
    {
        pthread_mutex_unlock(&heap_mutex);
        return (void *)((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE);
    }
}

void* heap_calloc_aligned_debug(size_t number, size_t size, int fileline, const char* filename)
{
    pthread_mutex_lock(&heap_mutex);

    if (number == 0 || size == 0 || heap_validate())
    {
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }

    void *p = heap_malloc_aligned_debug(number * size, fileline, filename);

    if (p != NULL) memset (p, 0, number*size);

    if (heap_validate())
    {
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }
    else
    {
        pthread_mutex_unlock(&heap_mutex);
        return p;
    }
}

void* heap_realloc_aligned_debug(void* memblock, size_t size, int fileline, const char* filename)
{
    if (memblock == NULL) return heap_malloc_aligned_debug(size, fileline, filename);

    if (size == 0)
    {
        heap_free(memblock);
        return NULL;
    }

    pthread_mutex_lock(&heap_mutex);

    if (heap_validate() || get_pointer_type(memblock) != pointer_valid)
    {
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }

    struct memblock_t *p = (struct memblock_t *)(((intptr_t )memblock - (MEMBLOCK_SIZE + FENCE_SIZE)));
    int size_to_copy = (p->size > size) ? (int)size : p->size;

    heap_free(memblock);
    void *new = heap_malloc_aligned_debug(size, fileline, filename);
    if (new != NULL) memcpy(new, memblock, size_to_copy);

    if (heap_validate())
    {
        pthread_mutex_unlock(&heap_mutex);
        return NULL;
    }
    else
    {
        pthread_mutex_unlock(&heap_mutex);
        return new;
    }
}

////////////////////////////////////////
/////// ------ S T A T S ------ ////////
////////////////////////////////////////

size_t heap_get_used_space(void)
{
    int size = 0;

    pthread_mutex_lock(&heap_mutex);
    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    while (p != NULL)
    {
        size += MEMBLOCK_SIZE;
        if (p->size != 0 && p->taken == 1) size += p->size + FENCE_SIZE * 2;
        p = p->next_block;
    }
    pthread_mutex_unlock(&heap_mutex);

    return (size_t)size;
}

size_t heap_get_largest_used_block_size(void)
{
    size_t size = 0;
    int max = 0;

    pthread_mutex_lock(&heap_mutex);
    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    while (p != NULL)
    {
        if (p->size > max)
        {
            max = p->size;
            size = (p->taken == 1) ? (size_t)p->size : 0;
        }
        p = p->next_block;
    }
    pthread_mutex_unlock(&heap_mutex);

    return size;
}

uint64_t heap_get_used_blocks_count(void)
{
    uint64_t used = 0;

    pthread_mutex_lock(&heap_mutex);
    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    while (p != NULL)
    {
        if (p->taken == 1 && p->size != 0) ++used;
        p = p->next_block;
    }
    pthread_mutex_unlock(&heap_mutex);

    return used;
}

size_t heap_get_free_space(void)
{
    int size = 0;

    pthread_mutex_lock(&heap_mutex);
    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    while (p != NULL)
    {
        if (p->taken == 0) size += p->size;
        p = p->next_block;
    }
    pthread_mutex_unlock(&heap_mutex);

    return (size_t)size;
}

size_t heap_get_largest_free_area(void)
{
    size_t size = heap_get_free_space();
    int max = 0;

    pthread_mutex_lock(&heap_mutex);
    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    while (p != NULL)
    {
        if (p->size > max && p->taken == 0)
        {
            max = p->size;
            size = (size_t)p->size;
        }
        p = p->next_block;
    }
    pthread_mutex_unlock(&heap_mutex);

    return size;
}

uint64_t heap_get_free_gaps_count(void)
{
    uint64_t counter = 0;

    pthread_mutex_lock(&heap_mutex);
    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    while (p != NULL)
    {
        if (p->taken == 0 && p->size >= WORD_SIZE + FENCE_SIZE*2) ++counter;
        p = p->next_block;
    }
    pthread_mutex_unlock(&heap_mutex);

    return counter;
}

////////////////////////////////////////
/////// ------ D U N N O ------ ////////
////////////////////////////////////////

enum pointer_type_t get_pointer_type(const const void* pointer)
{
    if (pointer == NULL) return pointer_null;
    pthread_mutex_lock(&heap_mutex);
    if ((intptr_t)pointer < the_Heap.start_brk)
    {
        pthread_mutex_unlock(&heap_mutex);
        return pointer_out_of_heap;
    }

    struct memblock_t * p = (struct memblock_t *)the_Heap.start_brk;
    intptr_t pointer_copy = (intptr_t)pointer;
    while (p != NULL)
    {
        if (pointer_copy < (intptr_t )p)
        {
            p = p->prev_block;
            if (p->taken == 1)
            {
                if (pointer_copy == (intptr_t)p || pointer_copy < (intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE) { pthread_mutex_unlock(&heap_mutex); return pointer_control_block; }
                if (pointer_copy == (intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE) { pthread_mutex_unlock(&heap_mutex); return pointer_valid; }
                if (pointer_copy < (intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE + p->size) { pthread_mutex_unlock(&heap_mutex); return pointer_inside_data_block; }
            }
            else
            {
                if (pointer_copy == (intptr_t )p || pointer_copy < (intptr_t)p + MEMBLOCK_SIZE) { pthread_mutex_unlock(&heap_mutex); return pointer_control_block; }
                if (pointer_copy == (intptr_t)p + MEMBLOCK_SIZE) { pthread_mutex_unlock(&heap_mutex); return pointer_valid; }
                if (pointer_copy < (intptr_t)p + MEMBLOCK_SIZE + p->size) { pthread_mutex_unlock(&heap_mutex); return pointer_unallocated; }
            }
        }
        p = p->next_block;
    }
    if (pointer_copy <= (the_Heap.brk - MEMBLOCK_SIZE)) { pthread_mutex_unlock(&heap_mutex); return pointer_control_block; }

    pthread_mutex_unlock(&heap_mutex);
    return pointer_out_of_heap;
}

void* heap_get_data_block_start(const void* pointer)
{
    pthread_mutex_lock(&heap_mutex);

    if (get_pointer_type(pointer) == pointer_valid) { pthread_mutex_unlock(&heap_mutex); return (void *)pointer; }
    if (get_pointer_type(pointer) != pointer_inside_data_block) { pthread_mutex_unlock(&heap_mutex); return NULL; }

    struct memblock_t * p = (struct memblock_t *)the_Heap.start_brk;
    intptr_t pointer_copy = (intptr_t)pointer;
    while (p != NULL)
    {
        if (pointer_copy < (intptr_t )p)
        {
            pthread_mutex_unlock(&heap_mutex);
            return (void *)((intptr_t )p->prev_block + MEMBLOCK_SIZE + FENCE_SIZE);
        }
        p = p->next_block;
    }
    pthread_mutex_unlock(&heap_mutex);

    return NULL;
}

size_t heap_get_block_size(const const void* memblock)
{
    pthread_mutex_lock(&heap_mutex);

    if ((get_pointer_type(memblock) != pointer_valid)) { pthread_mutex_unlock(&heap_mutex); return 0; }

    struct memblock_t * p = (struct memblock_t *)the_Heap.start_brk;
    intptr_t pointer_copy = (intptr_t)memblock;
    while (p != NULL)
    {
        if (pointer_copy < (intptr_t )p)
        {
            pthread_mutex_unlock(&heap_mutex);
            return (size_t)p->prev_block->size;
        }
        p = p->next_block;
    }
    pthread_mutex_unlock(&heap_mutex);

    return 0;
}

void heap_dump_debug_information(void)
{
    pthread_mutex_lock(&heap_mutex);
    if (heap_validate() == 0)
    {
        struct memblock_t *p = (struct memblock_t *) the_Heap.start_brk;
        printf("\n--------- MEMBLOCKS ---------\n");

        while (p != NULL)
        {
            printf("-----------------------------\n");
            printf("- address: %ld \n- size: %d \n- taken: %d", (intptr_t)p, p->size, p->taken);
            if (p->filename != NULL) printf("\n- filename: %s \n- fileline: %d", p->filename, p->fileline);
            printf("\n-----------------------------\n");
            p = p->next_block;
        }
        size_t heap_size = the_Heap.brk  - the_Heap.start_brk;
        pthread_mutex_unlock(&heap_mutex);

        printf("\n--------- HEAP INFO ---------\n");
        printf("- heap size: %zu \n- used space: %zu \n- free space: %zu \n- largest free: %zu", heap_size, heap_get_used_space(), heap_get_free_space(), heap_get_largest_free_area());
        printf("\n- sizeofs:\n   * HEAP_SIZE: %ld\n   * FENCE_SIZE: %ld\n   * MEMBLOCK_SIZE: %ld", sizeof(the_Heap), FENCE_SIZE, MEMBLOCK_SIZE);
        printf("\n-----------------------------\n");
    }
    else printf("\n-- THE HEAP IS DAMAGED! --\n");
    pthread_mutex_unlock(&heap_mutex);
}

int heap_validate(void)
{
    // -1 - heap not SET UP
    // -2 - invalid CRC
    // -3 - wrong HEAP DATA
    // -4 - messed up FENCES
    // -5 - invalid (out of heap) PRE && NEXT pointers

    pthread_mutex_lock(&heap_mutex);
    if ((void *)the_Heap.start_brk == NULL || (void *)the_Heap.brk == NULL)
    {
        pthread_mutex_unlock(&heap_mutex);
        return -1;
    }

    // HEAP CRC
    int prev_heap_crc = the_Heap.heap_crc;
    int new_heap_crc = the_Heap.heap_crc = 0;
    char *temp = (char *)the_Heap.start_brk;
    for (int i=0; i < (int)sizeof(the_Heap); ++i)
    {
        new_heap_crc += *(temp++);
    }
    the_Heap.heap_crc = new_heap_crc;
    if (prev_heap_crc != new_heap_crc)
    {
        pthread_mutex_unlock(&heap_mutex);
        return -2;
    }

    // HEAP DATA
    int used_space = heap_get_used_space();
    int largest_used = heap_get_largest_used_block_size();
    int used_blocks = heap_get_used_blocks_count();
    int free_space = heap_get_free_space();
    int largest_free = heap_get_largest_free_area();
    int free_blocks = heap_get_free_gaps_count();

    if (the_Heap.heap_used_space != used_space || the_Heap.heap_used_blocks != used_blocks || the_Heap.heap_largest_used != largest_used || the_Heap.heap_free_space != free_space || the_Heap.heap_free_blocks != free_blocks || the_Heap.heap_largest_free != largest_free)
    {
        pthread_mutex_unlock(&heap_mutex);
        return -3;
    }

    struct memblock_t *p = (struct memblock_t *) the_Heap.start_brk;
    while (p != NULL)
    {
        // FENCES
        if (p->taken && p->size != 0)
        {
            if (memcmp(fences.open, (void *)((intptr_t)p + MEMBLOCK_SIZE), FENCE_SIZE) != 0 || memcmp(fences.close, (void *)((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE + p->size), FENCE_SIZE) != 0)
            {
                pthread_mutex_unlock(&heap_mutex);
                return -4;
            }
        }

        // CRC
        int prev_crc = p->crc;
        update_crc(p);
        int new_crc = p->crc;
        if (prev_crc != new_crc)
        {
            pthread_mutex_unlock(&heap_mutex);
            return -2;
        }

        // PREV && NEXT
        if (get_pointer_type(p->prev_block) == pointer_out_of_heap || get_pointer_type(p->next_block) == pointer_out_of_heap)
        {
            pthread_mutex_unlock(&heap_mutex);
            return -5;
        }

        p = p->next_block;
    }
    pthread_mutex_unlock(&heap_mutex);

    return 0;
}

////////////////////////////////////////
/////// ------ M I N E ? ------ ////////
////////////////////////////////////////

void update_crc (struct memblock_t *p)
{
    if (p != NULL)
    {
        pthread_mutex_lock(&heap_mutex);
        p->crc = 0;
        char *temp = (char *)p;
        int new_crc = 0;
        for (int i=0; i < (int)MEMBLOCK_SIZE; ++i)
        {
            new_crc += *(temp++);
        }
        p->crc = new_crc;
        pthread_mutex_unlock(&heap_mutex);
        update_heap();
    }
}

void update_heap()
{
    pthread_mutex_lock(&heap_mutex);
    int used_space = heap_get_used_space();
    int largest_used = heap_get_largest_used_block_size();
    int used_blocks = heap_get_used_blocks_count();
    int free_space = heap_get_free_space();
    int largest_free = heap_get_largest_free_area();
    int free_blocks = heap_get_free_gaps_count();

    the_Heap.heap_used_space = used_space;
    the_Heap.heap_largest_used = largest_used;
    the_Heap.heap_used_blocks = used_blocks;
    the_Heap.heap_free_space = free_space;
    the_Heap.heap_largest_free = largest_free;
    the_Heap.heap_free_blocks = free_blocks;

    int temp_crc = the_Heap.heap_crc = 0;
    char *temp = (char *)the_Heap.start_brk;
    for (int i=0; i < (int)sizeof(the_Heap); ++i)
    {
        temp_crc += *(temp++);
    }
    the_Heap.heap_crc = temp_crc;
    pthread_mutex_unlock(&heap_mutex);
}

void heap_restart()
{
    pthread_mutex_lock(&heap_mutex);
    custom_sbrk(-1 * (the_Heap.brk - the_Heap.start_brk));
    the_Heap.start_brk = 0;
    the_Heap.brk = 0;
    the_Heap.heap_used_space = 0;
    the_Heap.heap_largest_used = 0;
    the_Heap.heap_used_blocks = 0;
    the_Heap.heap_free_space = 0;
    the_Heap.heap_largest_free = 0;
    the_Heap.heap_free_blocks = 0;
    pthread_mutex_unlock(&heap_mutex);

    pthread_mutexattr_destroy(&attr);
    pthread_mutex_destroy(&heap_mutex);
}