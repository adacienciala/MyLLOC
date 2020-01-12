#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mylloc.h"

#define WORD_SIZE sizeof(void *)
#define PAGE_SIZE 4096
#define FENCE_SIZE (sizeof(fences.open))
#define MEMBLOCK_SIZE (sizeof(struct memblock_t))
#define SET_FENCES(ptr, count) memcpy((void *)((intptr_t)ptr + MEMBLOCK_SIZE), fences.open, FENCE_SIZE); \
memcpy((void *)((intptr_t)ptr + MEMBLOCK_SIZE + FENCE_SIZE + count), fences.close, FENCE_SIZE)

// TO DO:
// - dorzuc crc
// - podwojne free
// - heap_get_block_size przy 1bajtowym bloku fails -> zmienic na petle

// REFACTOR:
// - int na size_t
// - intptr_t / void * / struct memblock_t * - ogarniecie lol

void print_heap(void)
{
    struct memblock_t *temp = (struct memblock_t *)the_Heap.start_brk;
    printf("\n \t\t\tTHE HEAP \n\n");
    printf("| sbrk: %ld |\n", the_Heap.start_brk);
    while (temp != NULL)
    {
        printf("| %ld - s: %d, t: %d ", temp, temp->size, temp->taken);
        temp = temp->next_block;
    }
    printf("\n| brk: %ld |\n", the_Heap.brk);
}

int heap_setup(void)
{
	the_Heap.start_brk = (intptr_t) custom_sbrk(MEMBLOCK_SIZE * 2);
	if ((void *)the_Heap.start_brk == NULL) return -1;

	the_Heap.brk = the_Heap.start_brk + MEMBLOCK_SIZE;

    ((struct memblock_t *)(the_Heap.start_brk))->next_block = (struct memblock_t *)(the_Heap.brk);
    ((struct memblock_t *)(the_Heap.start_brk))->prev_block = NULL;
    ((struct memblock_t *)(the_Heap.start_brk))->size = 0;
    ((struct memblock_t *)(the_Heap.start_brk))->taken = 1;

    ((struct memblock_t *)the_Heap.brk)->next_block = NULL;
    ((struct memblock_t *)the_Heap.brk)->prev_block = (struct memblock_t *)the_Heap.start_brk;
    ((struct memblock_t *)the_Heap.brk)->size = 0;
    ((struct memblock_t *)the_Heap.brk)->taken = 1;

    the_Heap.brk = the_Heap.brk + MEMBLOCK_SIZE;

    for (int i=0; i<(FENCE_SIZE/sizeof(fences.open[0])); ++i)
    {
        fences.open[i] = rand();
        fences.close[i] = rand();
    }

    printf("\nFENCE_SIZE: %ld, MEMBLOCK_SIZE: %ld\n\n", FENCE_SIZE, MEMBLOCK_SIZE);

	return 0;
}

void* heap_malloc(size_t count)
{
    if (count == 0) return NULL;
    // sprawdzic czy sterta nie jest uszkodzona

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
        if ((void *)the_Heap.brk == NULL) return NULL;

        the_Heap.brk = the_Heap.brk + count + FENCE_SIZE * 2;
        ((struct memblock_t *)the_Heap.brk)->size = 0;
        ((struct memblock_t *)the_Heap.brk)->taken = 1;
        ((struct memblock_t *)the_Heap.brk)->next_block = NULL;
        ((struct memblock_t *)the_Heap.brk)->prev_block = p;

        p->size = count;
        p->taken = 1;
        p->next_block = (struct memblock_t *)the_Heap.brk;
        SET_FENCES(p, count);
        the_Heap.brk = (the_Heap.brk) + MEMBLOCK_SIZE;

        return (void *)((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE);
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
    }

    p->size = (int)((intptr_t)p->next_block - ((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE*2));
    SET_FENCES(p, p->size);
    p->taken = 1;

    return (void *)((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE);
}

void* heap_calloc(size_t number, size_t size)
{
    if (number == 0 || size == 0) return NULL;

    void *p = heap_malloc(number * size);
    if (p != NULL) memset (p, 0, number*size);

    return p;
}

void heap_free(void* memblock)
{
    if (memblock != NULL)
    {
        struct memblock_t *p = (struct memblock_t *)((intptr_t )memblock - FENCE_SIZE - MEMBLOCK_SIZE);
        p->taken = 0;
        p->size += FENCE_SIZE*2;

        // coalesce eventual free neighbours
        if (p->prev_block->taken == 0)
        {
            p->prev_block->next_block = p->next_block;
            p->next_block->prev_block = p->prev_block;
            p->prev_block->size += (int)MEMBLOCK_SIZE + p->size;
            p = p->prev_block;
        }
        if (p->next_block->taken == 0)
        {
            p->size += (int)MEMBLOCK_SIZE + p->next_block->size;
            p->next_block = p->next_block->next_block;
            p->next_block->prev_block = p;
        }

        memblock = NULL;
    }
}

void* heap_realloc(void* memblock, size_t size)
{
    if (memblock == NULL) return heap_malloc(size);

    if (size == 0)
    {
        heap_free(memblock);
        return NULL;
    }

    struct memblock_t *p = (struct memblock_t *)(((intptr_t )memblock - (MEMBLOCK_SIZE + FENCE_SIZE)));
    int size_to_copy = (p->size > size) ? (int)size : p->size;

    heap_free(memblock);
    void *new = heap_malloc(size);
    if (new != NULL)
    {
        memcpy(new, memblock, size_to_copy);
    }

    return new;
}

////////////////////////////////////////
////// ------- D E B U G ------- ///////
////////////////////////////////////////

void* heap_malloc_debug(size_t count, int fileline, const char* filename)
{
    if (count == 0) return NULL;
    // sprawdzic czy sterta nie jest uszkodzona

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
        if ((void *)the_Heap.brk == NULL) return NULL;

        the_Heap.brk = the_Heap.brk + count + FENCE_SIZE * 2;
        ((struct memblock_t *)the_Heap.brk)->size = 0;
        ((struct memblock_t *)the_Heap.brk)->taken = 1;
        ((struct memblock_t *)the_Heap.brk)->next_block = NULL;
        ((struct memblock_t *)the_Heap.brk)->prev_block = p;
        ((struct memblock_t *)the_Heap.brk)->fileline = fileline;
        strcpy(((struct memblock_t *)the_Heap.brk)->filename, filename);

        p->size = count;
        p->taken = 1;
        p->next_block = (struct memblock_t *)the_Heap.brk;
        p->fileline = fileline;
        strcpy(p->filename, filename);
        SET_FENCES(p, count);
        the_Heap.brk = (the_Heap.brk) + MEMBLOCK_SIZE;

        return (void *)((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE);
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
        strcpy(((struct memblock_t *)temp)->filename, filename);
    }

    p->size = (int)((intptr_t)p->next_block - ((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE*2));
    SET_FENCES(p, p->size);
    p->taken = 1;
    p->fileline = fileline;
    strcpy(p->filename, filename);

    return (void *)((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE);
}

void* heap_calloc_debug(size_t number, size_t size, int fileline, const char* filename)
{
    if (number == 0 || size == 0) return NULL;

    void *p = heap_malloc(number * size);
    if (p != NULL) memset (p, 0, number*size);

    ((struct memblock_t *)((intptr_t )p - FENCE_SIZE - MEMBLOCK_SIZE))->fileline = fileline;
    strcpy(((struct memblock_t *)((intptr_t )p - FENCE_SIZE - MEMBLOCK_SIZE))->filename, filename);

    return p;
}
void* heap_realloc_debug(void* memblock, size_t size, int fileline, const char* filename)
{
    if (memblock == NULL) return heap_malloc_debug(size, fileline, filename);

    if (size == 0)
    {
        heap_free(memblock);
        return NULL;
    }

    struct memblock_t *p = (struct memblock_t *)(((intptr_t )memblock - (MEMBLOCK_SIZE + FENCE_SIZE)));
    int size_to_copy = (p->size > size) ? (int)size : p->size;

    heap_free(memblock);
    void *new = heap_malloc_debug(size, fileline, filename);
    if (new != NULL)
    {
        memcpy(new, memblock, size_to_copy);
    }

    return new;
}

////////////////////////////////////////
////// ----- A L I G N E D ----- ///////
////////////////////////////////////////

void* heap_malloc_aligned(size_t count)
{
    if (count == 0) return NULL;
    // sprawdzic czy sterta nie jest uszkodzona

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
        if ((void *)the_Heap.brk == NULL) return NULL;

        the_Heap.brk = the_Heap.brk + count + FENCE_SIZE * 2;
        ((struct memblock_t *)the_Heap.brk)->size = 0;
        ((struct memblock_t *)the_Heap.brk)->taken = 1;
        ((struct memblock_t *)the_Heap.brk)->next_block = NULL;
        ((struct memblock_t *)the_Heap.brk)->prev_block = p;

        p->size = count;
        p->taken = 1;
        p->next_block = (struct memblock_t *)the_Heap.brk;
        SET_FENCES(p, count);
        the_Heap.brk = (the_Heap.brk) + MEMBLOCK_SIZE;

        return (void *)((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE);
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
    }

    p->size = (int)((intptr_t)p->next_block - ((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE*2));
    SET_FENCES(p, p->size);
    p->taken = 1;

    return (void *)((intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE);
}

void* heap_calloc_aligned(size_t number, size_t size)
{
    if (number == 0 || size == 0) return NULL;

    void *p = heap_malloc(number * size);
    if (p != NULL) memset (p, 0, number*size);

    return p;
}

void* heap_realloc_aligned(void* memblock, size_t size)
{
    if (memblock == NULL) return heap_malloc(size);

    if (size == 0)
    {
        heap_free(memblock);
        return NULL;
    }

    struct memblock_t *p = (struct memblock_t *)(((intptr_t )memblock - (MEMBLOCK_SIZE + FENCE_SIZE)));
    int size_to_copy = (p->size > size) ? (int)size : p->size;

    heap_free(memblock);
    void *new = heap_malloc(size);
    if (new != NULL)
    {
        memcpy(new, memblock, size_to_copy);
    }

    return new;
}


////////////////////////////////////////
////// ----- A L I - D E B ----- ///////
////////////////////////////////////////

void* heap_malloc_aligned_debug(size_t count, int fileline, const char* filename);
void* heap_calloc_aligned_debug(size_t number, size_t size, int fileline, const char* filename);
void* heap_realloc_aligned_debug(void* memblock, size_t size, int fileline, const char* filename);

////////////////////////////////////////
/////// ------ S T A T S ------ ////////
////////////////////////////////////////

size_t heap_get_used_space(void)
{
    int size = 0;

    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    while (p != NULL)
    {
        size += MEMBLOCK_SIZE;
        if (p->size != 0 && p->taken == 1) size += p->size + FENCE_SIZE * 2;
        p = p->next_block;
    }

    return (size_t)size;
}

size_t heap_get_largest_used_block_size(void)
{
    size_t size;
    int max = 0;

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

    return size;
}

uint64_t heap_get_used_blocks_count(void)
{
    uint64_t used = 0;

    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    while (p != NULL)
    {
        if (p->taken == 1 && p->size != 0) ++used;
        p = p->next_block;
    }

    return used;
}

size_t heap_get_free_space(void)
{
    int size = 0;

    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    while (p != NULL)
    {
        if (p->taken == 0) size += p->size;
        p = p->next_block;
    }

    return (size_t)size;
}

size_t heap_get_largest_free_area(void)
{
    size_t size = heap_get_free_space();
    int max = 0;

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

    return size;
}

uint64_t heap_get_free_gaps_count(void)
{
    uint64_t counter = 0;

    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    while (p != NULL)
    {
        if (p->taken == 0 && p->size >= WORD_SIZE + FENCE_SIZE*2) ++counter;
        p = p->next_block;
    }

    return counter;
}

////////////////////////////////////////
/////// ------ D U N N O ------ ////////
////////////////////////////////////////

enum pointer_type_t get_pointer_type(const const void* pointer)
{
    if (pointer == NULL) return pointer_null;
    if (pointer < the_Heap.start_brk) return pointer_out_of_heap;

    struct memblock_t * p = (struct memblock_t *)the_Heap.start_brk;
    intptr_t pointer_copy = (intptr_t)pointer;
    while (p != NULL)
    {
        if (pointer_copy < (intptr_t )p)
        {
            p = p->prev_block;
            if (p->taken == 1)
            {
                if (pointer_copy == (intptr_t )p || pointer_copy < (intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE) return pointer_control_block;
                if (pointer_copy == (intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE) return pointer_valid;
                if (pointer_copy < (intptr_t)p + MEMBLOCK_SIZE + FENCE_SIZE + p->size) return pointer_inside_data_block;
            }
            else
            {
                if (pointer_copy == (intptr_t )p || pointer_copy < (intptr_t)p + MEMBLOCK_SIZE) return pointer_control_block;
                if (pointer_copy == (intptr_t)p + MEMBLOCK_SIZE) return pointer_valid;
                if (pointer_copy < (intptr_t)p + MEMBLOCK_SIZE + p->size) return pointer_unallocated;
            }
        }
        p = p->next_block;
    }
    if (pointer_copy < (the_Heap.brk - MEMBLOCK_SIZE)) return pointer_control_block;

    return pointer_out_of_heap;
}

void* heap_get_data_block_start(const void* pointer)
{
    if (get_pointer_type(pointer) == pointer_valid) return (void *)pointer;
    if (get_pointer_type(pointer) != pointer_inside_data_block) return NULL;

    struct memblock_t * p = (struct memblock_t *)the_Heap.start_brk;
    intptr_t pointer_copy = (intptr_t)pointer;
    while (p != NULL)
    {
        if (pointer_copy < (intptr_t )p)
        {
            return (void *)((intptr_t )p->prev_block + MEMBLOCK_SIZE + FENCE_SIZE);
        }
        p = p->next_block;
    }

    return NULL;
}

size_t heap_get_block_size(const const void* memblock)
{
    if (get_pointer_type(memblock) != pointer_valid) return 0;

    if (get_pointer_type((char *)memblock + 1) == pointer_unallocated) return (size_t)(((struct memblock_t *)(memblock - MEMBLOCK_SIZE))->size);
    else return (size_t)(((struct memblock_t *)(memblock - FENCE_SIZE - MEMBLOCK_SIZE))->size);
}

////////////////////////////////////////
/////// ------ T R A S H ------ ////////
////////////////////////////////////////

// jak ci sie chce to sie tak baw, ale latwiej bedzie nie tak xD
//void* heap_realloc(void* memblock, size_t size) //zapamietaj adres, free, malloc, przekopiuj ze starego adresu
//{
//    if (memblock == NULL) return heap_malloc(size);
//
//    if (size == 0)
//    {
//        heap_free(memblock);
//        return NULL;
//    }
//
//    struct memblock_t *p = (struct memblock_t *)(((intptr_t )memblock - (MEMBLOCK_SIZE + FENCE_SIZE)));
//    int size_to_copy = (p->size > size) ? (int)size : p->size;
//    int coalesced = 1;
//
//    // the user plays tricks with us
//    if ((int)size == p->size) return memblock;
//
//    // when the new size is smaller than the old size
//    if ((int)size < p->size)
//    {
//        // next block is free, so just expand it backwards
//        if (p->next_block->taken == 0)
//        {
//            memmove((void *)((intptr_t )p->next_block - FENCE_SIZE - (p->size - size)), (void *)((intptr_t )p->next_block - FENCE_SIZE), FENCE_SIZE + MEMBLOCK_SIZE);
//            p->next_block -= p->size - (int)size;
//            p->next_block->prev_block -= p->size - (int)size;
//            // takie sobie to memmove jak przesuwam strukture ze wskaznikami ;-;
//
//            p->next_block->size += p->size - (int)size;
//        }
//        // next block is in use, but we have space for a new block - handled beneath
//        // otherwise.. nothing? is it worth looking for a better fit or to temporarly "lose" sizeof(memblock)?
//    }
//    // when a neighbour is free and large enough
//    else if ((p->next_block->taken == 0 && ((p->size + p->next_block->size + (int)MEMBLOCK_SIZE) >= size)))
//    {
//        // shoot, co z plotkami in that fckn case bo jesli tu bedzie miejsce na nowy memblock? AAAAAAGH
//        p->size = p->next_block->size + p->size + (int)MEMBLOCK_SIZE;
//        p->next_block = p->next_block->next_block;
//        p->next_block->prev_block = p;
//        p->taken = 1;
//    }
//    else if (p->next_block->taken == 0 && p->prev_block->taken == 0 && ((p->size + p->next_block->size + p->prev_block->size + 2 * (int)MEMBLOCK_SIZE) >= size))
//    {
//        p->prev_block->next_block = p->next_block->next_block;
//        p->next_block->prev_block = p->prev_block;
//        memcpy((char *)p->prev_block + MEMBLOCK_SIZE, memblock, size_to_copy);
//        p->prev_block->size = p->next_block->size + p->prev_block->size + p->size + 2 * (int)MEMBLOCK_SIZE;
//        p->prev_block->taken = 1;
//        p = p->prev_block;
//    }
//    else coalesced = 0;
//
//    // add a new memblock, if enough space
//    if (coalesced == 1 && p->size > (int)(size + MEMBLOCK_SIZE))
//    {
//
//        intptr_t temp = (intptr_t)p + size + MEMBLOCK_SIZE;
//        ((struct memblock_t *)temp)->next_block = p->next_block;
//        p->next_block->prev_block = (struct memblock_t *)temp;
//
//        ((struct memblock_t *)temp)->prev_block = p;
//        p->next_block = (struct memblock_t *)temp;
//
//        ((struct memblock_t *)temp)->taken = 0;
//        ((struct memblock_t *)temp)->size = p->size - (int)(size + MEMBLOCK_SIZE);
//        for (int i = 0; i < 8; ++i)
//        {
//            ((struct memblock_t *)temp)->fence[i] = -1;
//        }
//    }
//
//    if (coalesced == 0)
//    {
//        intptr_t temp = (intptr_t) heap_malloc(size);
//        if ((void *)temp == NULL) return NULL;
//        memcpy((void *)temp, memblock, size_to_copy);
//        heap_free(memblock);
//        p = (struct memblock_t *)temp;
//    }
//
//    p->size = (int)((intptr_t)p->next_block - (intptr_t)p - MEMBLOCK_SIZE);
//    memblock = (void *)((intptr_t)p + MEMBLOCK_SIZE);
//
//    return memblock;
//}

