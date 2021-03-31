/**
 * Do not submit your assignment with a main function in this file.
 * If you submit with a main function in this file, you will get a zero.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "debug.h"
#include "sfmm.h"

void set_footer(sf_block *block){
    size_t block_header = block->header;
    size_t block_size = (block->header) >> 4 << 4;
    int shift;
    for( int i = 0; i < 8; i++){
        shift = i * 8;
        block->body.payload[block_size - 16 + i] = (block_header >> shift) & 0xff;
    }
}

void set_wilderness_block_links(sf_block *block){
    block->body.links.next = &sf_free_list_heads[7];
    block->body.links.prev = &sf_free_list_heads[7];
    sf_free_list_heads[7].body.links.next = block;
    sf_free_list_heads[7].body.links.prev = block;
}

void remove_link(sf_block *block){
    sf_block *prev_block = block->body.links.prev;
    sf_block *next_block = block->body.links.next;
    prev_block->body.links.next = next_block;
    next_block->body.links.prev = prev_block;
}

void set_block_link(sf_block *block){
    size_t m  = 32;
    size_t block_size = (block->header) >> 4 << 4;
    for (int i = 0; i < NUM_FREE_LISTS - 1; i++){
        if(block_size <= m || i == 6){
            sf_block *next_block = sf_free_list_heads[i].body.links.next;
            sf_free_list_heads[i].body.links.next = block;
            block->body.links.next = next_block;
            next_block->body.links.prev = block;
            block->body.links.prev = &sf_free_list_heads[i];
            return;
        }
        m <<= 1;
    }
}

void change_next_header(sf_block *block, int add, size_t amount){
    size_t block_size = (block->header) >> 4 << 4;
    sf_block *next_block = (void *)block + block_size;
    if(add){
        next_block->header += amount;
    }else{
        next_block->header -= amount;
    }
    
}

void *sf_malloc(size_t size) {
    
    if (size == 0){
        return NULL;
    }
    // check first malloc
    if(!sf_free_list_heads[0].body.links.next){
        for (int i = 0; i < NUM_FREE_LISTS; i++){
            sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
            sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
        }
        void *new_block_ptr = sf_mem_grow();

        // set up prologue and epilogue blocks
        sf_block *prologue = new_block_ptr + 8;
        prologue->header = 0x20 + THIS_BLOCK_ALLOCATED;
        set_footer(prologue);
        sf_block *epilogue = sf_mem_end() - sizeof(sf_header);
        epilogue->header = THIS_BLOCK_ALLOCATED;
        
        // add block to list 7
        sf_block *free_block = new_block_ptr + 40;
        free_block->header = 0x1fd2;
        set_footer(free_block);
        set_wilderness_block_links(free_block);
    }
    
    // calculate minimum size needed
    size_t min_block_size = size + 8;
    size_t to_add = min_block_size % 16;
    min_block_size += to_add ? 16 - to_add : 0;
    min_block_size = min_block_size < 32 ? 32 : min_block_size;
    // printf("\nmin size: %ld\n", min_block_size);
    
    // looping through each list to find block
    size_t m  = 32;
    for (int i = 0; i < NUM_FREE_LISTS; i++){
        // check if we have reach list 7
        if(i == 7){
            void *wilderness_block_ptr;
            if(sf_free_list_heads[i].body.links.next == &sf_free_list_heads[i]){
                wilderness_block_ptr = sf_mem_end() - sizeof(sf_header);
            }else{
                wilderness_block_ptr = sf_free_list_heads[i].body.links.next;
            }
            sf_block *malloc_block = wilderness_block_ptr;
            size_t wilderness_block_size = (malloc_block->header) >> 4 << 4;
            // keep expanding wilderness block until big enough
            while(wilderness_block_size < min_block_size){
                void *ptr = sf_mem_grow();
                if(!ptr){
                    sf_errno = ENOMEM;
                    return NULL;
                }
                malloc_block->header += 8192;
                wilderness_block_size += 8192;
                set_footer(malloc_block);
                sf_block *epilogue = sf_mem_end() - sizeof(sf_header);
                epilogue->header = THIS_BLOCK_ALLOCATED;
            }
            // if remaining size >= 32, make a cut, else don't
            if(wilderness_block_size >= (min_block_size+32)){
                size_t prev_alloc = malloc_block->header & 2;
                malloc_block->header = min_block_size + THIS_BLOCK_ALLOCATED + prev_alloc;
                sf_block *new_wilderness_block = wilderness_block_ptr + min_block_size;
                new_wilderness_block->header = wilderness_block_size - min_block_size + PREV_BLOCK_ALLOCATED;
                set_footer(new_wilderness_block);
                set_wilderness_block_links(new_wilderness_block);
            }else{
                malloc_block->header += THIS_BLOCK_ALLOCATED;
                sf_block *epilogue = sf_mem_end() - sizeof(sf_header);
                epilogue->header = THIS_BLOCK_ALLOCATED + PREV_BLOCK_ALLOCATED;
                sf_free_list_heads[i].body.links.next = &sf_free_list_heads[i];
                sf_free_list_heads[i].body.links.prev = &sf_free_list_heads[i];
            }
            return (void *)malloc_block + 8;
        }
        // check if size fits and have block
        else if((min_block_size <= m || i == 6) && (sf_free_list_heads[i].body.links.next != &sf_free_list_heads[i])){
            sf_block *free_block = sf_free_list_heads[i].body.links.next;
            size_t s;
            // search through list
            while(free_block != &sf_free_list_heads[i]){
                s = free_block->header >> 4 << 4;
                if(min_block_size+32 <= s){
                    size_t prev_alloc = free_block->header & 2;
                    free_block->header = min_block_size + THIS_BLOCK_ALLOCATED + prev_alloc;
                    sf_block *remain_block = (void *)free_block + min_block_size;
                    remain_block->header = s - min_block_size + PREV_BLOCK_ALLOCATED;
                    remove_link(free_block);
                    set_block_link(remain_block);
                    set_footer(remain_block);
                }else if(min_block_size <= s){
                    remove_link(free_block);
                    free_block->header += THIS_BLOCK_ALLOCATED;
                    change_next_header(free_block, 1, PREV_BLOCK_ALLOCATED);
                }
                return (void *)free_block + 8;
            }
        }
        m <<= 1;
    }
    return NULL;
}

void sf_free(void *pp) {
    // null pointer or not aligned pointer
    if(!pp){
        abort();
    }else if(((unsigned long)pp % 16) != 0){
        abort();
    }
    sf_block *to_free_block = pp-8;
    size_t block_size = (to_free_block->header) >> 4 << 4;
    // invalid size, or alloc bit
    if((block_size < 32) || (block_size%16 != 0) || !((to_free_block->header) & 1)){
        abort();
    }
    // if end of block > prologue
    if(((void *)to_free_block + block_size) > ((void *)sf_mem_end() - 8)){
        abort();
    }
    
    // check prev and next blocks
    sf_block *next_block = pp - 8 + block_size;
    size_t next_size = (next_block->header) >> 4 << 4;
    if(((void *)next_block + next_size) > ((void *)sf_mem_end() - 8)){
        // printf("Next Block is outside of heap\n");
        abort();
    }
    int coalesce_next = !(next_block->header & 1);
    int coalesce_prev = !(to_free_block->header & 2);
    // printf("\ncoalesce %d %d\n", coalesce_prev, coalesce_next);
    // if prev block and allocate bit doesn't agree
    if(coalesce_prev){
        sf_block *temp = pp-16;
        if(temp->header & 1){
            // printf("allocator bit doesn't agree abort");
            // fflush(stdout);
            abort();
        }
    }
    
    // do coalescing
    if(coalesce_next && coalesce_prev){
        sf_block *prev_footer = pp-16;
        size_t prev_size = prev_footer->header >> 4 << 4;
        sf_block *prev_block = pp-8-prev_size;
        // printf("\nsizes %ld %ld %ld\n", prev_size, block_size, next_size);
        if(next_block->body.links.next == &sf_free_list_heads[7]){
            // printf("reached wilderness block\n");
            prev_block->header +=  next_size + block_size;
            set_footer(prev_block);
            remove_link(prev_block);
            set_wilderness_block_links(prev_block);
        }else{
            prev_block->header +=  next_size + block_size;
            set_footer(prev_block);
            remove_link(prev_block);
            remove_link(next_block);
            set_block_link(prev_block);
        }
    }else if(coalesce_next){
        // printf("next block is free\n");
        if(next_block->body.links.next == &sf_free_list_heads[7]){
            // printf("reached wilderness block\n");
            to_free_block->header +=  next_size - THIS_BLOCK_ALLOCATED;
            set_footer(to_free_block);
            set_wilderness_block_links(to_free_block);
        }else{
            remove_link(next_block);
            to_free_block->header +=  next_size - THIS_BLOCK_ALLOCATED;
            set_footer(to_free_block);
            set_block_link(to_free_block);
        }
    }else if(coalesce_prev){
        sf_block *prev_footer = pp-16;
        size_t prev_size = prev_footer->header >> 4 << 4;
        sf_block *prev_block = pp-8-prev_size;
        prev_block->header += block_size;
        set_footer(prev_block);
        remove_link(prev_block);
        set_block_link(prev_block);
        change_next_header(prev_block, 0, PREV_BLOCK_ALLOCATED);
    }else{
        to_free_block->header -= THIS_BLOCK_ALLOCATED;
        set_footer(to_free_block);
        set_block_link(to_free_block);
        change_next_header(to_free_block, 0, PREV_BLOCK_ALLOCATED);
    }
    // printf("next block: %p %lu\n", next_block, next_size);

    return;
}

void *sf_realloc(void *pp, size_t rsize) {
    // null pointer or not aligned pointer
    if(!pp){
        abort();
    }else if(((unsigned long)pp % 16) != 0){
        abort();
    }
    sf_block *realloc_block = pp-8;
    size_t block_size = (realloc_block->header) >> 4 << 4;
    // invalid size, or alloc bit
    if((block_size < 32) || (block_size%16 != 0) || !((realloc_block->header) & 1)){
        abort();
    }
    // if end of block > prologue
    if(((void *)realloc_block + block_size) > ((void *)sf_mem_end() - 8)){
        abort();
    }
    // check prev and next blocks
    sf_block *next_block = pp - 8 + block_size;
    size_t next_size = (next_block->header) >> 4 << 4;
    if(((void *)next_block + next_size) > ((void *)sf_mem_end() - 8)){
        // printf("Next Block is outside of heap\n");
        abort();
    }
    int coalesce_prev = !(realloc_block->header & 2);
    // printf("\ncoalesce %d %d\n", coalesce_prev, coalesce_next);
    // if prev block and allocate bit doesn't agree
    if(coalesce_prev){
        sf_block *temp = pp-16;
        if(temp->header & 1){
            // printf("allocator bit doesn't agree abort");
            // fflush(stdout);
            abort();
        }
    }

    if(rsize == 0){
        sf_free(pp);
        return NULL;
    }

    if(rsize == block_size){
        return pp;
    }else if(rsize > block_size){
        void *new_ptr = sf_malloc(rsize);
        if(!new_ptr){
            return NULL;
        }
        memcpy(new_ptr, pp, block_size - 8);
        sf_free(pp);
        return new_ptr;
    }else{
        if((block_size - rsize - 8) < 32){
            return pp;
        }else{
            // calculate minimum size needed
            size_t min_block_size = rsize + 8;
            size_t to_add = min_block_size % 16;
            min_block_size += to_add ? 16 - to_add : 0;
            min_block_size = min_block_size < 32 ? 32 : min_block_size;

            realloc_block->header -= (block_size - min_block_size);
            sf_block *remain_block = pp + min_block_size - 8;
            remain_block->header = block_size - min_block_size + THIS_BLOCK_ALLOCATED + PREV_BLOCK_ALLOCATED;
            sf_free((void *)remain_block + 8);
            return pp;
        }
    }
}

void *sf_memalign(size_t size, size_t align) {
    if((align < 32) || (align & (align - 1))){
        sf_errno = EINVAL;
        return NULL;
    }
    // calculate minimum size needed for return block
    size_t min_block_size = size + 8;
    size_t to_add = min_block_size % 16;
    min_block_size += to_add ? 16 - to_add : 0;
    min_block_size = min_block_size < 32 ? 32 : min_block_size;
    
    // get a block from sf_malloc and split the front of the block
    size_t min_size = size + align + 40;
    void *malloc_ptr = sf_malloc(min_size);
    if(!malloc_ptr){
        return malloc_ptr;
    }

    sf_block *allocated_block = malloc_ptr - 8;
    size_t prev_alloc = allocated_block->header & 2;
    size_t allocated_size = allocated_block->header >> 4 << 4;
    size_t shift  = align - ((unsigned long)malloc_ptr % align);

    sf_block *return_block;
    if(shift == 0){
        return_block = allocated_block;
        return_block->header = min_block_size + THIS_BLOCK_ALLOCATED + prev_alloc;
    }else{
        shift = shift < 32 ? shift+align : shift;
        allocated_block->header = shift + prev_alloc + THIS_BLOCK_ALLOCATED;
        return_block = (void *)allocated_block + shift;
        return_block->header = allocated_size - shift + THIS_BLOCK_ALLOCATED + PREV_BLOCK_ALLOCATED;
        sf_free((void *)allocated_block + 8);
    }

    // if the back of the block is big enough to split
    if((allocated_size - min_block_size - shift) > 32){
        return_block->header = min_block_size + THIS_BLOCK_ALLOCATED;
        sf_block *remain_block = (void *)return_block + min_block_size;
        remain_block->header = allocated_size - min_block_size - shift + PREV_BLOCK_ALLOCATED + THIS_BLOCK_ALLOCATED;
        sf_free((void *)remain_block + 8);
    }

    return (void *)return_block + 8;
}
