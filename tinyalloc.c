/**
 * @file tinyalloc.c
 * @author Brais Solla González
 * @brief Tiny memory allocator for embedded or desktop systems with configurable arena
 * @version 0.3
 * @date 2021-09-07
 * 
 * @copyright Copyright (c) 2021
 * 
 */

#include "tinyalloc.h"

#define PADDING_SIZE (ALIGN_SIZE)

static inline size_t next_padding_size(size_t size){
    return (size + (PADDING_SIZE - 1) & ~(PADDING_SIZE - 1));
}

static void arena_lock(arena_t* arena_ptr){
    // pthread_mutex_lock(arena_ptr->arena_mutex);
}

static void arena_unlock(arena_t* arena_ptr){
    // pthread_mutex_unlock(arena_ptr->arena_mutex);
}

void arena_init(arena_t* arena_ptr){
    arena_ptr->head = NULL;
    arena_ptr->tail = NULL;

   // pthread_mutex_init(&arena_ptr->arena_mutex, NULL);
}

void arena_destroy(arena_t* arena_ptr){
    arena_ptr->head = NULL;
    arena_ptr->tail = NULL;
    arena_ptr->arena_size = 0;
    arena_ptr->start_addr = (void*) 0;

    // pthread_mutex_destroy(&arena_ptr->arena_mutex);
}

// Allocator helper functions

static inline canary_t compute_canary(allocator_header_t* header_ptr){
    return (canary_t) header_ptr->size ^ (uintptr_t) header_ptr->prev ^ (uintptr_t) header_ptr->next;
}

static inline void* pointer_end_block(allocator_header_t* header_ptr){
    // | HEADER | <- header_ptr 
    // | HEADER |
    // |  DATA  | <- header_ptr + HEADER_LENGTH
    // |  DATA  |
    // |  DATA  |
    // |  FREE  | <- header_ptr + HEADER_LENGTH + header_ptr->size
    // |  FREE  |
    // 
    // 
    return (void*) ((uintptr_t) header_ptr + HEADER_LENGHT + header_ptr->size);
}

static size_t available_block_space(arena_t* arena_ptr, allocator_header_t* header_ptr){
    if(header_ptr->next){
        return (size_t) ((uintptr_t) header_ptr->next - (uintptr_t) pointer_end_block(header_ptr));
    } else {
        // End of the linked-list
        return (size_t) ((uintptr_t) arena_ptr->start_addr + arena_ptr->arena_size) - (uintptr_t) pointer_end_block(header_ptr); 
    }
}


static inline allocator_header_t* ptr_to_header_ptr(void* ptr){
    return (allocator_header_t*) ((uintptr_t) ptr - HEADER_LENGHT);
}

static inline size_t compute_block_size(size_t padded_size){
    return padded_size + HEADER_LENGHT;
}

static inline void* header_to_dataptr(allocator_header_t* header_t){
    return (void*) ((uintptr_t) header_t + HEADER_LENGHT); 
}

void arena_info(arena_t* arena_ptr, arena_info_t* arena_info_ptr){

    //size_t total_size;
    //size_t used_size;
    //size_t allocated_size;
    //size_t fragmentation_bytes;
    //size_t allocated_blocks;

    arena_lock(arena_ptr);

    // TODO
    arena_info_ptr->total_size          = arena_ptr->arena_size;
    arena_info_ptr->used_size           = 0;
    arena_info_ptr->allocated_size      = 0;
    arena_info_ptr->fragmentation_bytes = 0;
    arena_info_ptr->allocated_blocks    = 0;

    if(arena_ptr->head){
        allocator_header_t* block = (allocator_header_t*) arena_ptr->head;
        allocator_header_t* last  = NULL;

        while(block){
            arena_info_ptr->allocated_size += block->size + HEADER_LENGHT;
            arena_info_ptr->allocated_blocks++;
            if(block->next) arena_info_ptr->fragmentation_bytes += available_block_space(arena_ptr, block);

            last  = block;
            block = (allocator_header_t*) block->next;
        }

        arena_info_ptr->used_size = ((uintptr_t) last + HEADER_LENGHT + last->size) - (uintptr_t) arena_ptr->start_addr;
    } else {
        arena_info_ptr->total_size = arena_ptr->arena_size;
    }

    arena_unlock(arena_ptr);
}


void* a_malloc(arena_t* arena_ptr, size_t size){
    // Allocate a free block in the arena. Size 0 is valid
    arena_lock(arena_ptr);

    size_t padded_size = next_padding_size(size);
    size_t block_size  = compute_block_size(padded_size);

    // Fix C++ compiler error
    allocator_header_t* actual = (allocator_header_t*) arena_ptr->head;
    allocator_header_t* prev   = NULL;

    // Wait a moment, what happens if head do NOT point to the start of the arena?
    if((uintptr_t) actual > (uintptr_t) arena_ptr->start_addr){
        // We have free space in the first location!
        size_t free_space_start = (uintptr_t) actual - (uintptr_t) arena_ptr->start_addr;

        if(free_space_start >= block_size){
            // Allocate a block on the start of the linked list and point arena to it!
            allocator_header_t* newblock = (allocator_header_t*) arena_ptr->start_addr;
            
            newblock->size    = padded_size;
            newblock->prev    = NULL;
            newblock->next    = arena_ptr->head;
            newblock->canary  = compute_canary(newblock);

            // Point the first block of the arena to this newly created one
            arena_ptr->head = newblock;

            arena_unlock(arena_ptr);
            return header_to_dataptr(newblock);
        }
    }


    if(actual){
        // Transverse the linked-list in search of a free block
        bool is_contiguous = false;

        while(actual->next){
            // TODO: Detect invalid canaries!
            if(available_block_space(arena_ptr, actual) >= block_size){
                // Contiguous space available for allocation!
                is_contiguous = true;
                break;
            }

            prev   = actual;
            // Fix C++ compiler error
            actual = (allocator_header_t*) actual->next; 
        }

        // Now actual is pointing to the block with free contiguous space
        // prev is the previous one. last header in the linked-list has the
        // next pointer to NULL

        allocator_header_t* newblock = (allocator_header_t*) pointer_end_block(actual);

        uintptr_t arena_end_ptr = (uintptr_t) arena_ptr->start_addr + arena_ptr->arena_size;

        // Wait a moment, do we have enought free memory in the arena?
        if(((uintptr_t) newblock + padded_size) > arena_end_ptr){
            // Nope, return NULL
            arena_unlock(arena_ptr);
            return NULL;
        }

        newblock->size   = padded_size;
        newblock->prev   = (void*) actual;
        newblock->next   = (void*) actual->next;
        newblock->canary = compute_canary(newblock);


        // Update previous block pointer
        actual->next = (void*) newblock;
        // Recompute previous block canary
        actual->canary = compute_canary(actual);

        if(newblock->next){
            // Update next block prev pointer to this element
            // Fix C++ compiler error
            allocator_header_t* next_ptr = (allocator_header_t*) newblock->next;
            next_ptr->prev = (void*) newblock;
            // Recompute next block canary
            next_ptr->canary = compute_canary(next_ptr);
        } else {
            // This is the last block in the linked list, update tail pointer
            arena_ptr->tail = newblock;
        }

        arena_unlock(arena_ptr);
        return header_to_dataptr(newblock);
    } else {
        // First block!
        if(block_size > arena_ptr->arena_size){
            arena_unlock(arena_ptr);
            return NULL; // Out of memory
        }
        // Allocate block
        allocator_header_t* newblock = (allocator_header_t*) arena_ptr->start_addr;

        newblock->size   = padded_size;
        newblock->prev   = NULL;
        newblock->next   = NULL;
        newblock->canary = compute_canary(newblock);

        // Update head and tail pointers
        arena_ptr->head = newblock;
        arena_ptr->tail = newblock;

        arena_unlock(arena_ptr);
        return header_to_dataptr(newblock);
    }
}


void* a_realloc(arena_t* arena_ptr, void* ptr, size_t size){
    if(!ptr) return a_malloc(arena_ptr, size);

    arena_lock(arena_ptr);

    // TODO: Verify canary!
    allocator_header_t* header_ptr = ptr_to_header_ptr(ptr);
    
    size_t newsize_padded   = next_padding_size(size);
    size_t actual_size      = header_ptr->size;

    if(newsize_padded <= actual_size){
        // Shrink data! This causes fragmentation!
        header_ptr->size   = newsize_padded;
        header_ptr->canary = compute_canary(header_ptr);
        arena_unlock(arena_ptr);
        return header_to_dataptr(header_ptr);
    } else {
        // We need more data, first check if available in contiguous region
        size_t available_to_next = available_block_space(arena_ptr, header_ptr);
        size_t required          = newsize_padded - actual_size;
        
        if(available_to_next >= required){
            // Nice, we have enough memory, resize block to new size
            header_ptr->size   = newsize_padded;
            header_ptr->canary = compute_canary(header_ptr);
            arena_unlock(arena_ptr);
            return header_to_dataptr(header_ptr);
        } else {
            // Not enought... Malloc / copy and free block
            // This creates A LOT of fragmentation
            // First, unlock the arena to allow relock via a_malloc function
            arena_unlock(arena_ptr);
            // Padding size will be maintained
            void* new_block = (void*) a_malloc(arena_ptr, newsize_padded);

            if(new_block){
                // Copy data to the new block atomically
                arena_lock(arena_ptr);
                memcpy(new_block, ptr, actual_size);
                arena_unlock(arena_ptr);
                // Free this block
                a_free(arena_ptr, ptr);
                
                return new_block;
            } else {
                return NULL;
            }
        }
    }
}

void a_free(arena_t* arena_ptr, void* ptr){
    if(!ptr) return; // Avoid NULL pointers? Will this be needed?
    arena_lock(arena_ptr);

    allocator_header_t* header_ptr = ptr_to_header_ptr(ptr);

    // Find next block and relink the linked-list
    if(header_ptr->prev){
        // Previous block exists
        allocator_header_t* header_prev = (allocator_header_t*) header_ptr->prev;
        header_prev->next = (void*) header_ptr->next;
    } else {
        // First block of the linked list!
        // FIX: What happens if we free the first block? Fragmentation on the start?
        // It's probably fixed, lets see 

        arena_ptr->head = (void*) header_ptr->next;
    }

    if(header_ptr->next){
        allocator_header_t* header_next = (allocator_header_t*) header_ptr->next;
        header_next->prev = header_ptr->prev;
    } else {
        // Update tail pointer, freeing last block
        arena_ptr->tail = header_ptr->prev;
    }

    arena_unlock(arena_ptr);
}