/**
 * @file first_fit.c
 * @brief First-fit allocation strategy implementation
 * @details Returns first block that satisfies size requirement
 */

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include "allocator.h"

/* Private functions */
static Block *find_first_fit_block(Block *head, size_t size);
static void coalesce_free_blocks(Block *head);
static Block *get_block_header(void *ptr);

/**
 * Initialize allocator with first-fit strategy
 */
Allocator *allocator_init(void) {
    Allocator *allocator = (Allocator *)malloc(sizeof(Allocator));
    if (!allocator) {
        fprintf(stderr, "ERROR: Failed to allocate allocator structure\n");
        return NULL;
    }

    allocator->heap = (char *)malloc(HEAP_SIZE);
    if (!allocator->heap) {
        fprintf(stderr, "ERROR: Failed to allocate heap memory\n");
        free(allocator);
        return NULL;
    }

    /* Initialize first block to cover entire heap */
    allocator->head = (Block *)allocator->heap;
    allocator->head->size = HEAP_SIZE - METADATA_SIZE;
    allocator->head->is_free = true;
    allocator->head->next = NULL;
    allocator->head->allocated_time = 0;
    allocator->head->allocator_thread = 0;

    /* Initialize mutex for thread safety */
    if (pthread_mutex_init(&allocator->lock, NULL) != 0) {
        fprintf(stderr, "ERROR: Failed to initialize mutex\n");
        free(allocator->heap);
        free(allocator);
        return NULL;
    }

    /* Initialize stats */
    allocator->stats.total_allocated = 0;
    allocator->stats.total_free = HEAP_SIZE - METADATA_SIZE;
    allocator->stats.num_allocations = 0;
    allocator->stats.num_blocks = 1;
    allocator->stats.fragmentation_ratio = 0;
    allocator->stats.failed_allocations = 0;

    return allocator;
}

/**
 * Find first-fit free block (O(n) worst case)
 * Returns first block that fits the requested size
 */
static Block *find_first_fit_block(Block *head, size_t size) {
    for (Block *current = head; current != NULL; current = current->next) {
        if (current->is_free && current->size >= size) {
            return current;
        }
    }
    return NULL;
}

/**
 * Coalesce adjacent free blocks into single blocks
 * Linear scan approach - O(n) time complexity
 */
static void coalesce_free_blocks(Block *head) {
    Block *current = head;

    while (current != NULL && current->next != NULL) {
        /* Check if current and next are both free and adjacent in memory */
        if (current->is_free && current->next->is_free) {
            /* Check if blocks are adjacent in memory */
            char *current_end = (char *)current + METADATA_SIZE + current->size;
            char *next_start = (char *)current->next;

            if (current_end == next_start) {
                /* Merge blocks */
                current->size += METADATA_SIZE + current->next->size;
                current->next = current->next->next;
                /* Don't advance current, check again for further coalescing */
                continue;
            }
        }
        current = current->next;
    }
}

/**
 * Allocate memory using first-fit strategy
 */
void *allocator_malloc(Allocator *allocator, size_t size) {
    if (!allocator || size == 0 || size > HEAP_SIZE) {
        return NULL;
    }

    pthread_mutex_lock(&allocator->lock);

    /* Find first-fit block */
    Block *block = find_first_fit_block(allocator->head, size);

    if (!block) {
        allocator->stats.failed_allocations++;
        pthread_mutex_unlock(&allocator->lock);
        return NULL;
    }

    /* Split block if there's excess space */
    if (block->size > size + METADATA_SIZE) {
        Block *new_block = (Block *)((char *)block + METADATA_SIZE + size);
        new_block->size = block->size - size - METADATA_SIZE;
        new_block->is_free = true;
        new_block->next = block->next;
        new_block->allocated_time = 0;
        new_block->allocator_thread = 0;

        block->size = size;
        block->next = new_block;
        allocator->stats.num_blocks++;
    }

    /* Mark block as allocated */
    block->is_free = false;
    block->allocated_time = time(NULL);
    block->allocator_thread = pthread_self();

    /* Update statistics */
    allocator->stats.total_allocated += size + METADATA_SIZE;
    allocator->stats.total_free -= (size + METADATA_SIZE);
    allocator->stats.num_allocations++;

    void *ptr = (char *)block + METADATA_SIZE;
    pthread_mutex_unlock(&allocator->lock);

    return ptr;
}

/**
 * Free allocated memory with coalescing
 */
void allocator_free(Allocator *allocator, void *ptr) {
    if (!allocator || !ptr) {
        return;
    }

    pthread_mutex_lock(&allocator->lock);

    Block *block = get_block_header(ptr);

    if (!block || block->is_free) {
        fprintf(stderr, "WARNING: Double free or invalid pointer detected\n");
        pthread_mutex_unlock(&allocator->lock);
        return;
    }

    /* Mark block as free */
    block->is_free = true;
    allocator->stats.total_allocated -= (block->size + METADATA_SIZE);
    allocator->stats.total_free += (block->size + METADATA_SIZE);
    allocator->stats.num_allocations--;

    /* Coalesce adjacent free blocks */
    coalesce_free_blocks(allocator->head);

    pthread_mutex_unlock(&allocator->lock);
}

/**
 * Reallocate memory to new size
 */
void *allocator_realloc(Allocator *allocator, void *ptr, size_t size) {
    if (!allocator) {
        return NULL;
    }

    if (!ptr) {
        return allocator_malloc(allocator, size);
    }

    if (size == 0) {
        allocator_free(allocator, ptr);
        return NULL;
    }

    pthread_mutex_lock(&allocator->lock);

    Block *block = get_block_header(ptr);
    if (!block || block->is_free) {
        pthread_mutex_unlock(&allocator->lock);
        return NULL;
    }

    /* If new size fits in current block */
    if (size <= block->size) {
        pthread_mutex_unlock(&allocator->lock);
        return ptr;
    }

    pthread_mutex_unlock(&allocator->lock);

    /* Allocate new block and copy data */
    void *new_ptr = allocator_malloc(allocator, size);
    if (new_ptr) {
        memcpy(new_ptr, ptr, block->size);
        allocator_free(allocator, ptr);
    }

    return new_ptr;
}

/**
 * Get block header from allocated pointer
 */
static Block *get_block_header(void *ptr) {
    if (!ptr) return NULL;
    return (Block *)((char *)ptr - METADATA_SIZE);
}

/**
 * Print heap status and visualization
 */
void print_heap_status(Allocator *allocator) {
    if (!allocator) return;

    pthread_mutex_lock(&allocator->lock);

    printf("\n=== HEAP STATUS (First-Fit Strategy) ===\n");
    printf("Total Heap Size: %d bytes\n", HEAP_SIZE);
    printf("Allocated: %zu bytes\n", allocator->stats.total_allocated);
    printf("Free: %zu bytes\n", allocator->stats.total_free);
    printf("Active Allocations: %zu\n", allocator->stats.num_allocations);
    printf("Total Blocks: %zu\n", allocator->stats.num_blocks);
    printf("Fragmentation: %zu%%\n", allocator->stats.fragmentation_ratio);
    printf("Failed Allocations: %zu\n\n", allocator->stats.failed_allocations);

    printf("Block Layout:\n");
    printf("+---------+---------+----------+\n");
    printf("| Address | Size    | Status   |\n");
    printf("+---------+---------+----------+\n");

    for (Block *current = allocator->head; current != NULL; current = current->next) {
        printf("| %p | %7zu | %-8s |\n",
               (void *)current,
               current->size,
               current->is_free ? "FREE" : "ALLOCATED");
    }
    printf("+---------+---------+----------+\n\n");

    pthread_mutex_unlock(&allocator->lock);
}

/**
 * Get allocator statistics
 */
AllocatorStats get_allocator_stats(Allocator *allocator) {
    if (!allocator) {
        AllocatorStats empty = {0};
        return empty;
    }

    pthread_mutex_lock(&allocator->lock);
    AllocatorStats stats = allocator->stats;
    pthread_mutex_unlock(&allocator->lock);

    return stats;
}

/**
 * Verify heap integrity
 */
bool allocator_verify(Allocator *allocator) {
    if (!allocator) return false;

    pthread_mutex_lock(&allocator->lock);

    for (Block *current = allocator->head; current != NULL; current = current->next) {
        /* Check if block pointer is within heap */
        if ((char *)current < allocator->heap ||
            (char *)current >= allocator->heap + HEAP_SIZE) {
            pthread_mutex_unlock(&allocator->lock);
            return false;
        }
        /* Check if size is valid */
        if (current->size == 0 || current->size > HEAP_SIZE) {
            pthread_mutex_unlock(&allocator->lock);
            return false;
        }
    }

    pthread_mutex_unlock(&allocator->lock);
    return true;
}

/**
 * Reset allocator state
 */
void allocator_reset(Allocator *allocator) {
    if (!allocator) return;

    pthread_mutex_lock(&allocator->lock);

    allocator->head = (Block *)allocator->heap;
    allocator->head->size = HEAP_SIZE - METADATA_SIZE;
    allocator->head->is_free = true;
    allocator->head->next = NULL;

    allocator->stats.total_allocated = 0;
    allocator->stats.total_free = HEAP_SIZE - METADATA_SIZE;
    allocator->stats.num_allocations = 0;
    allocator->stats.num_blocks = 1;
    allocator->stats.fragmentation_ratio = 0;
    allocator->stats.failed_allocations = 0;

    pthread_mutex_unlock(&allocator->lock);
}

/**
 * Destroy allocator and free resources
 */
void allocator_destroy(Allocator *allocator) {
    if (!allocator) return;

    pthread_mutex_destroy(&allocator->lock);
    free(allocator->heap);
    free(allocator);
}
