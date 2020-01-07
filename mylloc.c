#include <stdio.h>
#include <string.h>

#include "mylloc.h"

#define WORD_SIZE sizeof(void *)
#define FENCE_SIZE (sizeof(int) * 8)
#define SET_FENCES(ptr, count) memset((intptr_t)ptr + sizeof(struct memblock_t), 1, FENCE_SIZE); \
memset((intptr_t)ptr + sizeof(struct memblock_t) + FENCE_SIZE + count, 1, FENCE_SIZE)

// dorzuc crc, ogarnij  jak nie ma miejsca na strukture
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

    ((struct memblock_t *)the_Heap.brk)->next_block = NULL;
    ((struct memblock_t *)the_Heap.brk)->prev_block = (struct memblock_t *)the_Heap.start_brk;
    ((struct memblock_t *)the_Heap.brk)->size = 0;
    ((struct memblock_t *)the_Heap.brk)->taken = 1;

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
        if (p->size >= (count + FENCE_SIZE*2) && p->taken == 0)
        {
            found = 1;
            break;
        }
        p = p->next_block;
    }

    if (found == 0)
    {
        p = (struct memblock_t *)(the_Heap.brk - sizeof(struct memblock_t));
        the_Heap.brk = (intptr_t) custom_sbrk(sizeof(struct memblock_t) + FENCE_SIZE * 2 + count);
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
        the_Heap.brk = (the_Heap.brk) + sizeof(struct memblock_t);

        return (void *)((intptr_t)p + sizeof(struct memblock_t) + FENCE_SIZE);
    }

    // add a new memblock, if enough space
    if (p->size >= (int)(count + sizeof(struct memblock_t) + FENCE_SIZE*2))
    {
        intptr_t temp = (intptr_t)p + sizeof(struct memblock_t) + FENCE_SIZE*2 + count;
        ((struct memblock_t *)temp)->next_block = p->next_block;
        p->next_block->prev_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->prev_block = p;
        p->next_block = (struct memblock_t *)temp;

        ((struct memblock_t *)temp)->taken = 0;
        ((struct memblock_t *)temp)->size = p->size - (int)(sizeof(struct memblock_t) + FENCE_SIZE*2 + count);
    }

    p->size = (int)((intptr_t)p->next_block - ((intptr_t)p + sizeof(struct memblock_t) + FENCE_SIZE*2));
    SET_FENCES(p, p->size);
    p->taken = 1;

    return (void *)((intptr_t)p + sizeof(struct memblock_t) + FENCE_SIZE);
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
        struct memblock_t *p = (struct memblock_t *)((intptr_t )memblock - FENCE_SIZE - sizeof(struct memblock_t));
        p->taken = 0;
        p->size += FENCE_SIZE*2;

        // coalesce eventual free neighbours
        if (p->prev_block->taken == 0)
        {
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

void* heap_realloc(void* memblock, size_t size) // czy mniejsze? czy z kolejnym bedzie git? zapamietaj adres, free, malloc, przekopiuj ze starego adresu
// is it possible to overwrite that shit if memcpy to a prev block? is memcpy doing it byte by byte? lolz dunno
{
    if (memblock == NULL) return heap_malloc(size);

    if (size == 0)
    {
        heap_free(memblock);
        return NULL;
    }

    struct memblock_t *p = (struct memblock_t *)(((intptr_t )memblock - (sizeof(struct memblock_t) + FENCE_SIZE)));
    int size_to_copy = (p->size > size) ? (int)size : p->size;
    int coalesced = 1;

    // the user plays tricks with us
    if ((int)size == p->size) return memblock;

    // when the new size is smaller than the old size
    if ((int)size < p->size)
    {
        // next block is free, so just expand it backwards
        if (p->next_block->taken == 0)
        {
            memmove((void *)((intptr_t )p->next_block - FENCE_SIZE - (p->size - size)), (void *)((intptr_t )p->next_block - FENCE_SIZE), FENCE_SIZE + sizeof(struct memblock_t));
            p->next_block -= p->size - (int)size;
            p->next_block->prev_block -= p->size - (int)size;
            // takie sobie to memmove jak przesuwam strukture ze wskaznikami ;-;

            p->next_block->size += p->size - (int)size;
        }
        // next block is in use, but we have space for a new block - handled beneath
        // otherwise.. nothing? is it worth looking for a better fit or to temporarly "lose" sizeof(memblock)?
    }
    // when a neighbour is free and large enough
    else if ((p->next_block->taken == 0 && ((p->size + p->next_block->size + (int)sizeof(struct memblock_t)) >= size)))
    {
        // shoot, co z plotkami in that fckn case bo jesli tu bedzie miejsce na nowy memblock? AAAAAAGH
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
        heap_free(memblock);
        p = (struct memblock_t *)temp;
    }

    p->size = (int)((intptr_t)p->next_block - (intptr_t)p - sizeof(struct memblock_t));
    memblock = (void *)((intptr_t)p + sizeof(struct memblock_t));

    return memblock;
}