#include <stdio.h>
#include <string.h>

#include "mylloc.h"

#define heap_malloc(size) heap_malloc_debug(size, __LINE__, __FILE__)
#define heap_calloc(size, num) heap_calloc_debug(size, num, __LINE__, __FILE__)
#define heap_realloc(ptr, size) heap_realloc_debug(ptr, size, __LINE__, __FILE__)

#define heap_malloc_aligned(size) heap_malloc_aligned_debug(size, __LINE__, __FILE__)
#define heap_calloc_aligned(size, num) heap_calloc_aligned_debug(size, num, __LINE__, __FILE__)
#define heap_realloc_aligned(ptr, size) heap_realloc_aligned_debug(ptr, size, __LINE__, __FILE__)

void tests(void)
{
    // ----------- OVERALL TESTS ----------- //
    // [1] - multiple malloc allocations     //
    // [2] - same addresses after free       //
    // [3] - overwriting the fences          //
    // [4] - restarting the heap             //
    // [5] - calloc allocation               //
    // [6] - different free options          //
    // [7] - realloc function                //
    // [8] - malloc aligned function         //
    // [9] - calloc aligned function         //
    // [10] - realloc aligned function       //
    // [11] - thread safety                  //
    // ------------------------------------- //

    printf("\n---- START TESTS ----\n\n");

    // ---------------- [1] ---------------- //
    // -----multiple malloc allocations----- //

    int status = heap_setup();
    assert(status == 0);

    char** arr = (char **)heap_malloc(sizeof(char *) * 100);
    assert(arr != NULL);

    intptr_t old_p[100];

    for (int i = 0; i<100; ++i)
    {
        arr[i] = (char *)heap_malloc(sizeof(char) * ((i+1)*100));
        assert(arr[i] != NULL);
    }

    printf("-- test1 passed\n");

    // ---------------- [2] ---------------- //
    // ------same addresses after free------ //

    for (int i = 0; i<100; ++i)
    {
        old_p[i] = (intptr_t )arr[i];
        heap_free(arr[i]);
        assert(get_pointer_type(arr[i]) != pointer_valid);
    }

    for (int i = 0; i<100; ++i)
    {
        arr[i] = (char *)heap_malloc(sizeof(char) * ((i+1)*100));
        assert(arr[i] != NULL);
        assert(old_p[i] == (intptr_t)arr[i]);
    }

    printf("-- test2 passed\n");

    // ---------------- [3] ---------------- //
    // --------overwriting the fences------- //

    strcpy(arr[0], "");
    for (int i = 0; i<100; ++i)
    {
        strcat(arr[0], "a");
    }
    assert(heap_validate() != 0);

    printf("-- test3 passed\n");

    // ---------------- [4] ---------------- //
    // ---------restarting the heap--------- //

    heap_restart();
    assert((the_Heap.brk - the_Heap.start_brk) == 0);
    status = heap_setup();
    assert(status == 0);

    printf("-- test4 passed\n");

    // ---------------- [5] ---------------- //
    // ----------calloc allocation---------- //

    arr = (char **)heap_calloc(sizeof(char *), 100);
    assert(arr != NULL);

    for (int i = 0; i<100; ++i)
    {
        assert(arr[i] == NULL);
    }

    arr[0] = (char *)heap_malloc(sizeof(char)*100);
    assert(arr[0] != NULL);
    for (int i = 0; i < 100; ++i)
    {
        arr[0][i] = 'a';
    }
    heap_free(arr[0]);
    assert(get_pointer_type(arr[0]) != pointer_valid);
    arr[0] = (char *)heap_calloc(sizeof(char), 100);
    int total = 0;
    for (int j = 0; j < 100; ++j)
    {
        total += arr[0][j];
    }
    assert(total == 0);

    heap_restart();
    assert((the_Heap.brk - the_Heap.start_brk) == 0);
    status = heap_setup();
    assert(status == 0);

    printf("-- test5 passed\n");

    // ---------------- [6] ---------------- //
    // --------different free options------- //

    char* arr2[100];
    assert(heap_get_free_gaps_count() == 0);

    for (int i = 0; i<100; ++i)
    {
        arr2[i] = (char *)heap_malloc(sizeof(char) * 1000);
        assert(get_pointer_type(arr2[i]) == pointer_valid);
    }
    for (int i = 1; i<100; i += 4)
    {
        heap_free(arr2[i]);
        assert(get_pointer_type(arr2[i]) != pointer_valid);
    }
    assert(heap_get_free_gaps_count() == 25);
    for (int i = 0; i<100; i += 4)
    {
        heap_free(arr2[i]);
        assert(get_pointer_type(arr2[i]) != pointer_valid);
    }
    assert(heap_get_free_gaps_count() == 25);
    for (int i = 2; i<100; i += 4)
    {
        heap_free(arr2[i]);
        assert(get_pointer_type(arr2[i]) != pointer_valid);
    }
    assert(heap_get_free_gaps_count() == 25);
    for (int i = 3; i<100; i += 4)
    {
        heap_free(arr2[i]);
        assert(get_pointer_type(arr2[i]) != pointer_valid);
    }
    assert(heap_get_free_gaps_count() == 1);

    printf("-- test6 passed\n");

    // ---------------- [7] ---------------- //
    // -----------realloc function---------- //

    int *a = (int *)heap_realloc(NULL, sizeof(int));
    assert(a != NULL);
    *a = 666;
    assert(*a == 666);
    heap_realloc(a, 0);
    assert(get_pointer_type(a) != pointer_valid);
    assert(heap_get_used_blocks_count() == 0 && heap_validate() == 0);

    int tab[5] = {1, 2, 3, 4, 5};
    int *aa = (int *)heap_realloc(NULL, sizeof(int) * 5);
    assert(aa != NULL);
    memcpy(aa, tab, sizeof(tab));
    aa = heap_realloc(aa, sizeof(char) * 3);
    assert(aa != NULL);
    assert(memcmp(aa, tab, 3) == 0);
    assert(memcmp(aa, tab, 5) != 0);
    aa = heap_realloc(aa, sizeof(char) * 5);
    assert(aa != NULL);
    assert(memcmp(aa, tab, 3) == 0);

    heap_restart();
    assert((the_Heap.brk - the_Heap.start_brk) == 0);
    status = heap_setup();
    assert(status == 0);

    printf("-- test7 passed\n");

    // ---------------- [8] ---------------- //
    // -------malloc aligned function------- //

    printf("-- test8 passed\n");

    // ---------------- [9] ---------------- //
    // -------calloc aligned function------- //

    printf("-- test9 passed\n");

    // ---------------- [10] --------------- //
    // -------realloc aligned function------ //

    printf("-- test10 passed\n");

    // ---------------- [11] --------------- //
    // ------------thread safety------------ //

    printf("-- test11 passed\n");


    printf("\n---- TESTS  DONE ----\n");
}

int main(int argc, char **argv)
{
    tests();

    return 0;
}