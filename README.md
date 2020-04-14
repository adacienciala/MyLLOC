# MyLLOC
My own memory allocator (known as 'dark magic operator'). 

Well, not exactly. It's an memory manager to control the program's heap with the individual implementation of malloc, calloc, realloc and free functions. But that one liner *rhymed*, so it stayed.

## Requirements and running

We're using custom sbrk function instead of the built-in one. **Don't use the default one or the standard functions to allocate memory!**

Compile the program with:
```
make
```

Run the tests with:
```
./mylloc
```

## About
<div align="center">
  <img src="https://user-images.githubusercontent.com/57063056/79276700-c7ce1a00-7ea8-11ea-9665-86b844dba357.png"/>
</div> 
<br /> 

The program runs tests of the 4 main functions implemented: 
* heap_malloc
* heap_calloc
* heap_realloc
* heap_free 

And a few additional ones. See API's description below.

The main functions have the **aligned versions**, where memory is allocated along the start of a memory page, and the **debug versions** to help with, well, debugging. 

## Tests
<div align="center">
  <img src="https://user-images.githubusercontent.com/57063056/79276701-c7ce1a00-7ea8-11ea-889c-ec60b0339144.png"/>
</div> 
<br />
There are 11 tests implemented. They allocate and free memory in the most complicated way I could think of at the moment. 

1. **\[Multiple malloc allocations]**\
*check, if malloc can allocate different sizes without a problem*
2. **\[Perfect fits]** \
*free the previously allocated memory and try to allocate it again, shooting for the perfect fit*
3. **\[Overwriting the fences]** \
*check, if the heap is protected by breaching the fences*
4. **\[Restarting the heap]** \
*test of the heap_setup function*
5. **\[Calloc allocation]** \
*calloc part of the memory that is known to have something already written and check, if it wrote 0s*
6. **\[Different free options]** \
*check, how free manages coalescing free space (preventing fragmentation)*
7. **\[Realloc function]** \
*check, if realloc can allocate without a problem*
8. **\[Malloc aligned function]** \
*test of the heap_malloc_aligned function; see test 1*
9. **\[Calloc aligned function]** \
*test of the heap_calloc_aligned function; see test 5*
10. **\[Realloc aligned function]** \
*test of the heap_realloc_aligned function; see test 7*
11. **\[Thread safety]** \
*different allocations within multiple threads*

## Goals
Main goals of the program:
1. It works.
2. Thread safety of the API.
3. Ability to reset the heap to the starting state.
4. Self-expanding heap by requesting the OS for memory.

## API description
Beside 4 main functions and their aligned and debug versions, there are additional functions:
* *int* **heap_setup** *(void)*\
initializes the heap
* *size_t* **heap_get_used_space** *(void)*\
gets the number of used bytes (allocated blocks + inner structs)
* *size_t* **heap_get_largest_used_block_size** *(void)*\
gets the size of the largest allocated block
* *uint64_t* **heap_get_used_blocks_count** *(void)*\
gets the number of allocated blocks
* *size_t* **heap_get_free_space** *(void)*\
gets the number of free bytes
* *size_t* **heap_get_largest_free_area** *(void)*\
gets the size of the largest space available to allocation (without structs)
* *uint64_t* **heap_get_free_gaps_count** *(void)*\
gets the number of gaps available to allocation (min. structs + CPU word size)
* *enum* **pointer_type_t get_pointer_type** *(const const void\* pointer)*\
gets the type of the pointer:\
-&nbsp; *pointer_null* - NULL\
-&nbsp; *pointer_out_of_heap* - not in the heap\
-&nbsp; *pointer_control_block* - in an inner struct or a fence\
-&nbsp; *pointer_inside_data_block* - in an allocated block; not the first byte\
-&nbsp; *pointer_unallocated* - in an unallocated block\
-&nbsp; *pointer_valid* - in an allocated block; the first byte
* *void\** **heap_get_data_block_start** *(const void\* pointer)*\
gets a pointer to the block that the *pointer* is in
* *size_t* **heap_get_block_size** *(const const void\* memblock)*\
gets the size of the block
* *void* **heap_dump_debug_information** *(void)*\
print all the blocks in order and their info:\
-&nbsp; starting address\
-&nbsp; size for/of the data\
-&nbsp; taken or free to allocate\
-&nbsp; *\[in debug]* filename of the file in which the block was allocated\
-&nbsp; *\[in debug]* fileline in which the block was allocated\
print heap's info:
-&nbsp; size (got from the OS)\
-&nbsp; used space (structs + data)\
-&nbsp; free space (avaiable to allocation)\
-&nbsp; largest free block (avaiable to allocation)\
-&nbsp; sizes of the heap, fence and block's structs\
* *int* **heap_validate** *(void)*\
checks if:\
-&nbsp; the heap is set up\
-&nbsp; the checksum is valid\
-&nbsp; the get functions are valid\
-&nbsp; the fences are not breached\
-&nbsp; all memory blocks are accesible

* *void* **update_crc** *(struct memblock_t \*p)*\
recalculates the checksums
* *void* **update_heap** *(void)*\
updates the heap's info and recalculates its checksum
* *void* **heap_restart** *(void)*\
restarts the size of the heap to 0 (releases program's memory to the OS)

## Notes
Things I learned thanks to this project:
* [x] how dynamic allocation works
* [x] writing tests
* [x] recursive mutex does magic
