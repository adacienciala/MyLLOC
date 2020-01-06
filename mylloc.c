#include <stdio.h>
#include <string.h>

#include "custom_unistd.h"

#define WORD_SIZE sizeof(void *)

struct memblock_t
{
	struct memblock_t *prev_block;
    struct memblock_t *next_block;
    int size;
    int taken;
    int fence[8];
};

struct memory_list_t
{
    intptr_t start_brk;
    intptr_t brk;
};

struct memory_list_t the_Heap;

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
	the_Heap.start_brk = (intptr_t) custom_sbrk(sizeof(struct memblock_t) * 2);
	if ((void *)the_Heap.start_brk == NULL) return -1;

	the_Heap.brk = the_Heap.start_brk + sizeof(struct memblock_t);

    ((struct memblock_t *)(the_Heap.start_brk))->next_block = (struct memblock_t *)(the_Heap.brk);
    ((struct memblock_t *)(the_Heap.start_brk))->prev_block = NULL;
    ((struct memblock_t *)(the_Heap.start_brk))->size = 0;
    ((struct memblock_t *)(the_Heap.start_brk))->taken = 1;
    for (int i = 0; i < 8; ++i)
    {
        ((struct memblock_t *)(the_Heap.start_brk))->fence[i] = -1;
    }

    ((struct memblock_t *)the_Heap.brk)->next_block = NULL;
    ((struct memblock_t *)the_Heap.brk)->prev_block = (struct memblock_t *)the_Heap.start_brk;
    ((struct memblock_t *)the_Heap.brk)->size = 0;
    ((struct memblock_t *)the_Heap.brk)->taken = 1;
    for (int i = 0; i < 8; ++i)
    {
        ((struct memblock_t *)(the_Heap.brk))->fence[i] = -1;
    }

    the_Heap.brk = the_Heap.brk + sizeof(struct memblock_t);

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
        if (p->size >= count && p->taken == 0)
        {
            found = 1;
            break;
        }
        p = p->next_block;
    }

    if (found == 0)
    {
        p = (struct memblock_t *)(the_Heap.brk - sizeof(struct memblock_t));
        the_Heap.brk = (intptr_t) custom_sbrk(sizeof(struct memblock_t) + count);
        if ((void *)the_Heap.brk == NULL) return NULL;

        the_Heap.brk = (the_Heap.brk) + count;
        ((struct memblock_t *)the_Heap.brk)->size = 0;
        ((struct memblock_t *)the_Heap.brk)->taken = 1;
        ((struct memblock_t *)the_Heap.brk)->next_block = NULL;
        ((struct memblock_t *)the_Heap.brk)->prev_block = p;
        for (int i = 0; i < 8; ++i)
        {
            ((struct memblock_t *)(the_Heap.brk))->fence[i] = -1;
        }

        p->size = count;
        p->taken = 1;
        p->next_block = (struct memblock_t *)the_Heap.brk;
        the_Heap.brk = (the_Heap.brk) + sizeof(struct memblock_t);

        return (void *)((intptr_t)p + sizeof(struct memblock_t));
    }

    // add a new memblock, if enough space
    if (p->size > (int)(count + sizeof(struct memblock_t)))
    {
        intptr_t temp = (intptr_t)p + count + sizeof(struct memblock_t);
        ((struct memblock_t *)temp)->next_block = p->next_block;
        p->next_block->prev_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->prev_block = p;
        p->next_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->taken = 0;
        ((struct memblock_t *)temp)->size = p->size - (int)(count + sizeof(struct memblock_t));
        for (int i = 0; i < 8; ++i)
        {
            ((struct memblock_t *)temp)->fence[i] = -1;
        }
    }

    p->size = (int)((intptr_t)p->next_block - (intptr_t)p - sizeof(struct memblock_t));
    p->taken = 1;

    return (void *)((intptr_t)p + sizeof(struct memblock_t));
}

void* heap_calloc(size_t number, size_t size)
{
    if (number == 0 || size == 0) return NULL;

    intptr_t p = (intptr_t ) heap_malloc(number * size);
    if ((void *)p != NULL) memset ((void *)p, 0, number*size);

    return (void *)p;
}

void heap_free(void* memblock)
{
    if (memblock != NULL)
    {
        struct memblock_t *p = (struct memblock_t *)(memblock - sizeof(struct memblock_t));
        p->taken = 0;

        // coalesce eventual free neighbours
        if (p->prev_block->taken == 0)
        {
            if (p->prev_block != NULL) ;
            if (p->next_block != NULL) printf("JEEEAJ");
            p->prev_block->next_block = p->next_block;
            p->next_block->prev_block = p->prev_block;
            p->prev_block->size += (int)sizeof(struct memblock_t) + p->size;
            p = p->prev_block;
        }
        if (p->next_block->taken == 0)
        {
            p->size += (int)sizeof(struct memblock_t) + p->next_block->size;
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

    int size_to_copy = ((intptr_t )(memblock + size) > the_Heap.brk) ? (int)(the_Heap.brk - (intptr_t )memblock) : (int)size;

    struct memblock_t *p = (struct memblock_t *)(memblock - sizeof(struct memblock_t));
    int coalesced = 1;
    // when new size is smaller than the old size
    if (p->size > size)
    {
        // next block is free, so just expand it backwards
        if (p->next_block->taken == 0)
        {
            intptr_t temp = (intptr_t)p + size + sizeof(struct memblock_t);
            ((struct memblock_t *)temp)->next_block = p->next_block->next_block;
            ((struct memblock_t *)temp)->prev_block = p->next_block->prev_block;
            ((struct memblock_t *)temp)->size = p->next_block->size + (p->size - (int)size);
            ((struct memblock_t *)temp)->taken = p->next_block->taken;
            for (int i = 0; i < 8; ++i)
            {
                ((struct memblock_t *)temp)->fence[i] = -1;
            }

            p->next_block->next_block->prev_block = (struct memblock_t *)temp;
            p->next_block = (struct memblock_t *)temp;
        }
        // next block is in use, but we have space for a new block
        // otherwise.. nothing?
    }
    // when a neighbour is free and large enough
    else if ((p->prev_block->taken == 0 && ((p->size + p->prev_block->size + (int)sizeof(struct memblock_t)) >= size)))
    {
        p->prev_block->next_block = p->next_block;
        p->next_block->prev_block = p->prev_block;
        memcpy((char *)p->prev_block + sizeof(struct memblock_t), memblock, size_to_copy);
        p->prev_block->size = p->prev_block->size + p->size + (int)sizeof(struct memblock_t);
        p->prev_block->taken = 1;
        p = p->prev_block;
    }
    else if ((p->next_block->taken == 0 && ((p->size + p->next_block->size + (int)sizeof(struct memblock_t)) >= size)))
    {
        p->size = p->next_block->size + p->size + (int)sizeof(struct memblock_t);
        p->next_block = p->next_block->next_block;
        p->next_block->prev_block = p;
        p->taken = 1;
    }
    else if (p->next_block->taken == 0 && p->prev_block->taken == 0 && ((p->size + p->next_block->size + p->prev_block->size + 2 * (int)sizeof(struct memblock_t)) >= size))
    {
        p->prev_block->next_block = p->next_block->next_block;
        p->next_block->prev_block = p->prev_block;
        memcpy((char *)p->prev_block + sizeof(struct memblock_t), memblock, size_to_copy);
        p->prev_block->size = p->next_block->size + p->prev_block->size + p->size + 2 * (int)sizeof(struct memblock_t);
        p->prev_block->taken = 1;
        p = p->prev_block;
    }
    else coalesced = 0;

    // add a new memblock, if enough space
    if (coalesced == 1 && p->size > (int)(size + sizeof(struct memblock_t)))
    {
        printf("LOL");

        intptr_t temp = (intptr_t)p + size + sizeof(struct memblock_t);
        ((struct memblock_t *)temp)->next_block = p->next_block;
        p->next_block->prev_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->prev_block = p;
        p->next_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->taken = 0;
        ((struct memblock_t *)temp)->size = p->size - (int)(size + sizeof(struct memblock_t));
        for (int i = 0; i < 8; ++i)
        {
            ((struct memblock_t *)temp)->fence[i] = -1;
        }
    }

    if (coalesced == 0)
    {
        intptr_t temp = (intptr_t) heap_malloc(size);
        if ((void *)temp == NULL) return NULL;
        memcpy((void *)temp, memblock, size_to_copy);
        p = (struct memblock_t *)temp;
    }

    p->size = (int)((intptr_t)p->next_block - (intptr_t)p - sizeof(struct memblock_t));
    memblock = (void *)((intptr_t)p + sizeof(struct memblock_t));

    return memblock;
}

void* heap_malloc_debug(size_t count, int fileline, const char* filename)
{
    if (count == 0) return NULL;
    // sprawdzic czy sterta nie jest uszkodzona

    struct memblock_t *p = (struct memblock_t *)the_Heap.start_brk;
    int found = 0;
    while (p != NULL)
    {
        if (p->size >= count && p->taken == 0)
        {
            found = 1;
            break;
        }
        p = p->next_block;
    }

    if (found == 0)
    {
        p = (struct memblock_t *)(the_Heap.brk - sizeof(struct memblock_t));
        the_Heap.brk = (intptr_t) custom_sbrk(sizeof(struct memblock_t) + count);
        if ((void *)the_Heap.brk == NULL) return NULL;

        the_Heap.brk = (the_Heap.brk) + count;
        ((struct memblock_t *)the_Heap.brk)->size = 0;
        ((struct memblock_t *)the_Heap.brk)->taken = 1;
        ((struct memblock_t *)the_Heap.brk)->next_block = NULL;
        ((struct memblock_t *)the_Heap.brk)->prev_block = p;
        for (int i = 0; i < 8; ++i)
        {
            ((struct memblock_t *)(the_Heap.brk))->fence[i] = -1;
        }

        p->size = count;
        p->taken = 1;
        p->next_block = (struct memblock_t *)the_Heap.brk;
        the_Heap.brk = (the_Heap.brk) + sizeof(struct memblock_t);

        printf("\n\n%d:: MALLOC (\"%s\")\n", fileline, filename);
        return (void *)((intptr_t)p + sizeof(struct memblock_t));
    }

    // add a new memblock, if enough space
    if (p->size > (int)(count + sizeof(struct memblock_t)))
    {
        intptr_t temp = (intptr_t)p + count + sizeof(struct memblock_t);
        ((struct memblock_t *)temp)->next_block = p->next_block;
        p->next_block->prev_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->prev_block = p;
        p->next_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->taken = 0;
        ((struct memblock_t *)temp)->size = p->size - (int)(count - sizeof(struct memblock_t));
        for (int i = 0; i < 8; ++i)
        {
            ((struct memblock_t *)temp)->fence[i] = -1;
        }
    }

    printf("\n\n%d:: MALLOC (\"%s\")\n", fileline, filename);
    return (void *)((intptr_t)p + sizeof(struct memblock_t));
}

void* heap_calloc_debug(size_t number, size_t size, int fileline, const char* filename)
{
    if (number == 0 || size == 0) return NULL;

    intptr_t p = (intptr_t ) heap_malloc(number * size);
    if ((void *)p != NULL) memset ((void *)p, 0, number*size);

    printf("\n\n%d:: CALLOC (\"%s\")\n", fileline, filename);
    return (void *)p;
}

void* heap_realloc_debug(void* memblock, size_t size, int fileline, const char* filename)
{
    if (memblock == NULL) return heap_malloc(size);

    if (size == 0)
    {
        heap_free(memblock);
        return NULL;
    }

    int size_to_copy = ((intptr_t )(memblock + size) > the_Heap.brk) ? (int)(the_Heap.brk - (intptr_t )memblock) : (int)size;

    struct memblock_t *p = (struct memblock_t *)(memblock - sizeof(struct memblock_t));
    int coalesced = 1;
    // when new size is smaller than the old size
    if (p->size > size_to_copy)
    {
        // next block is free, so just expand it backwards
        if (p->next_block->taken == 0)
        {
            intptr_t temp = (intptr_t)p + size + sizeof(struct memblock_t);
            ((struct memblock_t *)temp)->next_block = p->next_block->next_block;
            ((struct memblock_t *)temp)->prev_block = p->next_block->prev_block;
            ((struct memblock_t *)temp)->size = p->next_block->size + (p->size - (int)size);
            ((struct memblock_t *)temp)->taken = p->next_block->taken;
            for (int i = 0; i < 8; ++i)
            {
                ((struct memblock_t *)temp)->fence[i] = -1;
            }

            p->next_block->next_block->prev_block = (struct memblock_t *)temp;
            p->next_block = (struct memblock_t *)temp;
        }
        // next block is in use, but we have space for a new block
        // otherwise.. nothing?
    }
        // when a neighbour is free and large enough
    else if ((p->prev_block->taken == 0 && ((p->size + p->prev_block->size + (int)sizeof(struct memblock_t)) >= size)))
    {
        p->prev_block->next_block = p->next_block;
        p->next_block->prev_block = p->prev_block;
        memcpy((char *)p->prev_block + sizeof(struct memblock_t), memblock, size_to_copy);
        p->prev_block->size = p->prev_block->size + p->size + (int)sizeof(struct memblock_t);
        p->prev_block->taken = 1;
        p = p->prev_block;
    }
    else if ((p->next_block->taken == 0 && ((p->size + p->next_block->size + (int)sizeof(struct memblock_t)) >= size)))
    {
        p->size = p->next_block->size + p->size + (int)sizeof(struct memblock_t);
        p->next_block = p->next_block->next_block;
        p->next_block->prev_block = p;
        p->taken = 1;
    }
    else if (p->next_block->taken == 0 && p->prev_block->taken == 0 && ((p->size + p->next_block->size + p->prev_block->size + 2 * (int)sizeof(struct memblock_t)) >= size))
    {
        p->prev_block->next_block = p->next_block->next_block;
        p->next_block->prev_block = p->prev_block;
        memcpy((char *)p->prev_block + sizeof(struct memblock_t), memblock, size_to_copy);
        p->prev_block->size = p->next_block->size + p->prev_block->size + p->size + 2 * (int)sizeof(struct memblock_t);
        p->prev_block->taken = 1;
        p = p->prev_block;
    }
    else coalesced = 0;

    // add a new memblock, if enough space
    if (coalesced == 1 && p->size > (int)(size + sizeof(struct memblock_t)))
    {
        intptr_t temp = (intptr_t)p + size + sizeof(struct memblock_t);
        ((struct memblock_t *)temp)->next_block = p->next_block;
        p->next_block->prev_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->prev_block = p;
        p->next_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->taken = 0;
        ((struct memblock_t *)temp)->size = p->size - (int)(size - sizeof(struct memblock_t));
        for (int i = 0; i < 8; ++i)
        {
            ((struct memblock_t *)temp)->fence[i] = -1;
        }
    }

    if (coalesced == 0)
    {
        intptr_t temp = (intptr_t) heap_malloc(size);
        if ((void *)temp == NULL) return NULL;
        memcpy((void *)temp, memblock, size_to_copy);
        p = (struct memblock_t *)temp;
    }

    memblock = (void *)((intptr_t)p + sizeof(struct memblock_t));

    printf("\n\n%d:: REALLOC (\"%s\")\n", fileline, filename);
    return memblock;
}
