#include <stdio.h>
#include <string.h>

#include "mylloc.h"

#define heap_malloc(size) heap_malloc_debug(size, __LINE__, __FILE__)
#define heap_calloc(size, num) heap_calloc_debug(size, num, __LINE__, __FILE__)
#define heap_realloc(ptr, size) heap_realloc_debug(ptr, size, __LINE__, __FILE__)

#define heap_malloc_aligned(size) heap_malloc_aligned_debug(size, __LINE__, __FILE__)
#define heap_calloc_aligned(size, num) heap_calloc_aligned_debug(size, num, __LINE__, __FILE__)
#define heap_realloc_aligned(ptr, size) heap_realloc_aligned_debug(ptr, size, __LINE__, __FILE__)


int main(int argc, char **argv)
{
    heap_setup();
    char* p = (char *)heap_malloc_aligned(sizeof(char)*1000);
    heap_dump_debug_information();
    char* pp = heap_malloc(5);
    heap_dump_debug_information();
    heap_free(p);
    heap_dump_debug_information();
    char* ppp = (char *)heap_malloc_aligned(sizeof(char)*5);
    heap_dump_debug_information();
    printf("\nVALIDATE: %d\n", heap_validate());

    heap_restart();
    heap_setup();
    char* pr = (char *)heap_malloc_aligned(sizeof(char)*1000);
    heap_dump_debug_information();
    printf("\nVALIDATE: %d\n", heap_validate());

    return 0;
}

#if 0
#include "custom_unistd.h"

int main(int argc, char **argv) {
	int status = heap_setup();
	assert(status == 0);

	// parametry pustej sterty
	size_t free_bytes = heap_get_free_space();
	size_t used_bytes = heap_get_used_space();

	void* p1 = heap_malloc(8 * 1024 * 1024); // 8MB
	void* p2 = heap_malloc(8 * 1024 * 1024); // 8MB
	void* p3 = heap_malloc(8 * 1024 * 1024); // 8MB
	void* p4 = heap_malloc(16 * 1024 * 1024); // 16MB
	assert(p1 != NULL); // malloc musi się udać
	assert(p2 != NULL); // malloc musi się udać
	assert(p3 != NULL); // malloc musi się udać
	assert(p4 == NULL); // nie ma prawa zadziałać

	status = heap_validate();
	assert(status == 0); // sterta nie może być uszkodzona

	// zaalokowano 3 bloki
	assert(heap_get_used_blocks_count() == 3);

	// zajęto 24MB sterty; te 2000 bajtów powinno
    // wystarczyć na wewnętrzne struktury sterty
	assert(
        heap_get_used_space() >= 24 * 1024 * 1024 &&
        heap_get_used_space() <= 24 * 1024 * 1024 + 2000
        );

	// zwolnij pamięć
	heap_free(p1);
	heap_free(p2);
	heap_free(p3);

	// wszystko powinno wrócić do normy
	assert(heap_get_free_space() == free_bytes);
	assert(heap_get_used_space() == used_bytes);

	// już nie ma bloków
	assert(heap_get_used_blocks_count() == 0);

	return 0;
}
#endif