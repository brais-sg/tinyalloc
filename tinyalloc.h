/**
 * @file tinyalloc.h
 * @author Brais Solla González
 * @brief Tiny memory allocator for embedded or desktop systems with configurable arena
 * @version 0.3
 * @date 2021-09-07
 * 
 * @copyright Copyright (c) 2021
 * 
 */


#ifndef _TINYALLOC_INCLUDED
#define _TINYALLOC_INCLUDED

// debug
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

#define WORDSIZE    (sizeof(void*))
#define ALIGN_SIZE  ((int) (WORDSIZE))
// TODO: 4*WORDSIZE to use only 16 bytes on 32 bit systems
// #define HEADER_LENGHT 32
// DONE: 4*WORDSIZE!
#define HEADER_LENGHT (4 * WORDSIZE)


typedef uint8_t ALIGN[HEADER_LENGHT];
typedef size_t canary_t;

// arena
typedef struct {
    void*   start_addr;
    size_t  arena_size;
    void*   head;
    void*   tail;

    // pthread_mutex_t arena_mutex;
} arena_t;

typedef struct {
    size_t total_size;
    size_t used_size;
    size_t allocated_size;
    size_t fragmentation_bytes;

    size_t allocated_blocks;
} arena_info_t;


union allocator_header {
    struct {
        canary_t canary;
        size_t  size; // data size
        void*   prev;
        void*   next;
    };
    ALIGN align;
};

typedef union allocator_header allocator_header_t;

// arena functions

/**
 * @brief Prepares the arena for usage
 * 
 * @param arena_ptr 
 */
void arena_init(arena_t* arena_ptr);

/**
 * @brief Releases the arena
 * 
 * @param arena_ptr 
 */
void arena_destroy(arena_t* arena_ptr);

/**
 * @brief Fills an arena_info_t struct with information about the allocator and the arena
 * 
 * @param arena_ptr Pointer to the arena struct
 * @param arena_info_ptr Pointer to the info struct to be filled
 */
void arena_info(arena_t* arena_ptr, arena_info_t* arena_info_ptr);



// Allocator functions

/**
 * @brief Allocates memory in the arena pointed by arena_ptr
 * 
 * @param arena_ptr Pointer to the arena_t struct previously initialized
 * @param size Size of the memory to allocate in bytes. It will be padded to the nearest ISA word size
 * @return void* Pointer to memory allocated or NULL on error (Example: Not enought memory)
 * 
 * The allocator will use the first-fit algorithm
 */
void* a_malloc(arena_t* arena_ptr, size_t size);

/**
 * @brief Reallocates memory in the arena pointed by arena_ptr
 * 
 * @param arena_ptr Pointer to the arena_t struct previously initialized
 * @param ptr Pointer to the previously allocated block or NULL to perform an a_malloc operation
 * @param size Size of the memory to rellocate in bytes. It will be padded to the nearest ISA word size
 * @return void* New pointer to the block, or NULL on error. The new pointer can be different to the last block pointer
 * 
 * The allocator will use the first-fit algorithm, but only if the block couldn't be resized to the new size
 */
void* a_realloc(arena_t* arena_ptr, void* ptr, size_t size);

/**
 * @brief Frees memory in the arena pointed by arena_ptr
 * 
 * @param arena_ptr Pointer to the arena_t struct previously initialized
 * @param ptr Valid pointer previously returned from a_malloc or a_realloc
 * 
 * NOTE: This can cause memory fragmentation
 */
void  a_free(arena_t* arena_ptr, void* ptr);

#endif 
