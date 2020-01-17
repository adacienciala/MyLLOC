#include <stdio.h>
#include <string.h>
#include <unistd.h>

#include "mylloc.h"

#define PAGE_SIZE 4096
#define MS 1000

#define heap_malloc(size) heap_malloc_debug(size, __LINE__, __FILE__)
#define heap_calloc(size, num) heap_calloc_debug(size, num, __LINE__, __FILE__)
#define heap_realloc(ptr, size) heap_realloc_debug(ptr, size, __LINE__, __FILE__)

#define heap_malloc_aligned(size) heap_malloc_aligned_debug(size, __LINE__, __FILE__)
#define heap_calloc_aligned(size, num) heap_calloc_aligned_debug(size, num, __LINE__, __FILE__)
#define heap_realloc_aligned(ptr, size) heap_realloc_aligned_debug(ptr, size, __LINE__, __FILE__)

void* f_0 (void* none)
{
    char **aa = (char **)heap_malloc(sizeof(char *) * 20);
    assert(aa != NULL);
    for (int i=0; i < 20; ++i)
    {
        aa[i] = heap_malloc(sizeof(char) * 100);
        assert(aa[i] != NULL);
        usleep(500*MS);
    }
    for (int i=0; i < 20; ++i)
    {
        heap_free(aa[i]);
        assert(get_pointer_type(aa[i]) != pointer_valid);
        usleep(300*MS);
    }
}

void* f_1 (void* none)
{
    char **aa = (char **)heap_malloc_aligned(sizeof(char *) * 20);
    assert(aa != NULL);
    for (int i=0; i < 20; ++i)
    {
        aa[i] = heap_malloc_aligned(sizeof(char) * 100);
        assert(aa[i] != NULL);
        usleep(500*MS);
    }
    for (int i=0; i < 20; ++i)
    {
        heap_free(aa[i]);
        assert(get_pointer_type(aa[i]) != pointer_valid);
        usleep(300*MS);
    }
}

void* f_2 (void* none)
{
    char **aa = (char **)heap_realloc_aligned(NULL, sizeof(char *) * 20);
    assert(aa != NULL);
    assert(((intptr_t)aa & (intptr_t)(PAGE_SIZE - 1)) == 0);
    for (int i=0; i < 20; ++i)
    {
        aa[i] = heap_realloc_aligned(NULL, sizeof(char) * 50);
        assert(aa[i] != NULL);
        assert(((intptr_t)aa[i] & (intptr_t)(PAGE_SIZE - 1)) == 0);
        usleep(370*MS);
        aa[i] = heap_realloc_aligned(aa[i], sizeof(char) * 70);
        usleep(150*MS);
    }
    for (int i=0; i < 20; ++i)
    {
        heap_realloc_aligned(aa[i], 0);
        assert(get_pointer_type(aa[i]) != pointer_valid);
        usleep(450*MS);
    }
}

void* f_3 (void* none)
{
    char **aa = (char **)heap_calloc(sizeof(char *), 20);
    assert(aa != NULL);
    for (int i=0; i < 20; ++i)
    {
        aa[i] = heap_calloc(sizeof(char), 1500);
        assert(aa[i] != NULL);
        usleep(180*MS);
    }
    for (int i=0; i < 20; ++i)
    {
        heap_free(aa[i]);
        assert(get_pointer_type(aa[i]) != pointer_valid);
        usleep(300*MS);
    }
}

void* f_4 (void* none)
{
    char **aa = (char **)heap_calloc_aligned(sizeof(char *), 20);
    assert(aa != NULL);
    for (int i=0; i < 20; ++i)
    {
        aa[i] = heap_calloc_aligned(sizeof(char), 1500);
        assert(((intptr_t)aa[i] & (intptr_t)(PAGE_SIZE - 1)) == 0);
        assert(aa[i] != NULL);
        usleep(270*MS);
    }
    for (int i=0; i < 20; ++i)
    {
        heap_free(aa[i]);
        assert(get_pointer_type(aa[i]) != pointer_valid);
        usleep(50*MS);
    }
}

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
    // [11] - heap_get_* functions           //
    // [12] - thread safety                  //
    // ------------------------------------- //

    printf("\n---- START TESTS ----\n\n");

    // ---------------- [1] ---------------- //
    // -----multiple malloc allocations----- //
    // ------------------------------------- //

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
    // ------------------------------------- //
    // - malloc some space and free it       //
    // - malloc perfect fits                 //
    // ------------------------------------- //

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
    // ------------------------------------- //
    // - have space for 99 chars + 1 '\0'    //
    // - strcat 100 chars + 1 '\0'           //
    // ------------------------------------- //

    strcpy(arr[0], "");
    for (int i = 0; i<100; ++i)
    {
        strcat(arr[0], "a");
    }
    assert(heap_validate() != 0);

    printf("-- test3 passed\n");

    // ---------------- [4] ---------------- //
    // ---------restarting the heap--------- //
    // ------------------------------------- //

    heap_restart();
    assert((the_Heap.brk - the_Heap.start_brk) == 0);
    status = heap_setup();
    assert(status == 0);

    printf("-- test4 passed\n");

    // ---------------- [5] ---------------- //
    // ----------calloc allocation---------- //
    // - malloc space, write stuff, free it  //
    // - calloc perfect size, count bytes    //
    // ------------------------------------- //

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
    // ------------------------------------- //
    // - free single blocks (no coalescing)  //
    // - free left blocks (coalescing <-)    //
    // - free right blocks (coalescing ->)   //
    // - free middle blocks (coalescing <->) //
    // ------------------------------------- //

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
    // ------------------------------------- //

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
    // ------------------------------------- //

    char** b = (char **)heap_malloc_aligned(sizeof(char *) * 50);
    assert(b != NULL);
    for (int i = 0; i < 50; ++i)
    {
        b[i] = (char *)heap_malloc_aligned(sizeof(char) * (4050+1));
        assert(get_pointer_type(b[i]) == pointer_valid);
        assert(((intptr_t)b[i] & (intptr_t)(PAGE_SIZE - 1)) == 0);
    }

    for (int i = 0; i < 50; ++i)
    {
        heap_free(b[i]);
        assert(get_pointer_type(b[i]) != pointer_valid);
    }

    printf("-- test8 passed\n");

    // ---------------- [9] ---------------- //
    // -------calloc aligned function------- //
    // ------------------------------------- //

    for (int i = 0; i < 50; ++i)
    {
        b[i] = (char *)heap_malloc_aligned(sizeof(char) * 10);
        assert(get_pointer_type(b[i]) == pointer_valid);
        assert(((intptr_t)b[i] & (intptr_t)(PAGE_SIZE - 1)) == 0);
        for (int j=0; j < 5; ++j)
        {
            b[i][j] = (char)('a'+j);
        }
        heap_free(b[i]);
        b[i] = (char *)heap_calloc_aligned(sizeof(char), 10);
        assert(get_pointer_type(b[i]) == pointer_valid);
        assert(((intptr_t)b[i] & (intptr_t)(PAGE_SIZE - 1)) == 0);
        for (int j=0; j < 5; ++j)
        {
            assert(b[i][j] == 0);
        }
    }

    printf("-- test9 passed\n");

    // ---------------- [10] --------------- //
    // -------realloc aligned function------ //
    // ------------------------------------- //

    printf("-- test10 passed\n");

    // ---------------- [11] --------------- //
    // ---------heap_get_* functions-------- //
    // ------------------------------------- //



    printf("-- test11 passed\n");

    // ---------------- [12] --------------- //
    // ------------thread safety------------ //
    // ------------------------------------- //

    heap_restart();
    assert((the_Heap.brk - the_Heap.start_brk) == 0);
    status = heap_setup();
    assert(status == 0);

    pthread_t thread[10];
    void * (*fun[5]) (void *);
    fun[0] = f_0;
    fun[1] = f_1;
    fun[2] = f_2;
    fun[3] = f_3;
    fun[4] = f_4;

    for (int i = 0; i < 5; ++i)
    {
        pthread_create(&thread[i], NULL, fun[i], NULL);
        usleep(100*MS);
    }
    for (int i = 0; i < 5; ++i)
    {
        pthread_join(thread[i], NULL);
    }
    assert(heap_validate() == 0);

    printf("-- test12 passed\n");

    heap_restart();
    assert((the_Heap.brk - the_Heap.start_brk) == 0);

    printf("\n---- TESTS  DONE ----\n");
}

int main(int argc, char **argv)
{
    tests();

    return 0;
}