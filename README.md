# Dynamic Memory Allocator
I have created an allocator for the x86-64 architecture with the following features:

- Free lists segregated by size class, using first-fit policy within each size class.
- Immediate coalescing of large blocks on free with adjacent free blocks.
- Boundary tags to support efficient coalescing, with footer optimization that allows footers to be omitted from allocated blocks.
- Block splitting without creating splinters.
- Allocated blocks aligned to "double memory row" (16-byte) boundaries.
- Free lists maintained using **last in first out (LIFO)** discipline.
- "Wilderness block" heuristic, to avoid unnecessary growing of the heap.

The functions that I implemented are: **malloc**, **realloc**, **free**, and **memalign** functions.

## sf_malloc
```c
  /*
   * It acquires uninitialized memory that is aligned and padded properly for the underlying system.
   *
   * @param size The number of bytes requested to be allocated.
   *
   * @return If size is 0, then NULL is returned without setting sf_errno.
   * If size is nonzero, then if the allocation is successful a pointer to a valid region of
   * memory of the requested size is returned.  If the allocation is not successful, then
   * NULL is returned and sf_errno is set to ENOMEM.
   */
  void *sf_malloc(size_t size);
```

## sf_realloc
```c
  /*
   * Resizes the memory pointed to by ptr to size bytes.
   *
   * @param ptr Address of the memory region to resize.
   * @param size The minimum size to resize the memory to.
   *
   * @return If successful, the pointer to a valid region of memory is
   * returned, else NULL is returned and sf_errno is set appropriately.
   *
   * If sf_realloc is called with an invalid pointer sf_errno should be set to EINVAL.
   * If there is no memory available sf_realloc should set sf_errno to ENOMEM.
   *
   * If sf_realloc is called with a valid pointer and a size of 0 it should free
   * the allocated block and return NULL without setting sf_errno.
   */
  void *sf_realloc(void *ptr, size_t size);
```

## sf_free
```c
  /*
   * Marks a dynamically allocated region as no longer in use.
   * Adds the newly freed block to the free list.
   *
   * @param ptr Address of memory returned by the function sf_malloc.
   *
   * If ptr is invalid, the function calls abort() to exit the program.
   */
  void sf_free(void *ptr);
```

## sf_memalign
```c
  /*
   * Allocates a block of memory with a specified alignment.
   *
   * @param align The alignment required of the returned pointer.
   * @param size The number of bytes requested to be allocated.
   *
   * @return If align is not a power of two or is less than the minimum block size,
   * then NULL is returned and sf_errno is set to EINVAL.
   * If size is 0, then NULL is returned without setting sf_errno.
   * Otherwise, if the allocation is successful a pointer to a valid region of memory
   * of the requested size and with the requested alignment is returned.
   * If the allocation is not successful, then NULL is returned and sf_errno is set
   * to ENOMEM.
   */
  void *sf_memalign(size_t size, size_t align);
```

## Helper Functions

I have used the following helper functions from the `sfutil` library to help me visualize my
free lists and allocated blocks.

```c
  void sf_show_block(sf_block *bp);
  void sf_show_free_list(int index);
  void sf_show_free_lists();
  void sf_show_heap();
```

## Compiling

In the root directory, executing `make` or `make all` will compile
anything that needs to be, `make debug` does the same except that it compiles the code
with options suitable for debugging, and `make clean` removes files that resulted from
a previous compilation.

## Executing Main()

Executing `bin/sfmm` after compiling will run the main function from `src\main.c`. The usage of the **malloc**, **realloc**,
**free**, and **memalign** functions can be done in  `main()`.

## Unit Testing

I have used a C unit testing framework called [Criterion](https://github.com/Snaipe/Criterion).
The tests are located in the `tests` directory. After compiling, you can run the tests by executing `bin/birp_tests`. 
