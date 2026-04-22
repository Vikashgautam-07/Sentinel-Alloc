/**
 * @file allocator.h
 * @brief Custom memory allocator with thread-safe operations
 * @details Implements first-fit and best-fit allocation strategies
 *          with linear scan coalescing for fragmentation management
 */

#ifndef CUSTOM_ALLOCATOR_H
#define CUSTOM_ALLOCATOR_H

#include <stddef.h>
#include <stdbool.h>
#include <pthread.h>
#include <time.h>

/* Configuration Constants */
#define HEAP_SIZE (1024*1024)
#define MIN_BLOCK_SIZE sizeof(Block)
#define METADATA_SIZE sizeof(Block)

/**
 * @struct Block
 * @brief Memory block metadata header
 */
typedef struct Block {
    size_t size;              /**< Size of usable payload (excluding header) */
    bool is_free;             /**< Availability flag */
    struct Block *next;       /**< Next block in linked list */
    time_t allocated_time;    /**< Timestamp of allocation */
    pid_t allocator_thread;   /**< Thread that allocated this block */
} Block;

/**
 * @struct AllocatorStats
 * @brief Statistics and metrics for allocator performance
 */
typedef struct {
    size_t total_allocated;   /**< Total bytes currently allocated */
    size_t total_free;        /**< Total bytes currently free */
    size_t num_allocations;   /**< Number of active allocations */
    size_t num_blocks;        /**< Total number of blocks */
    size_t fragmentation_ratio; /**< Fragmentation percentage */
    size_t failed_allocations; /**< Count of failed allocation requests */
} AllocatorStats;

/**
 * @struct Allocator
 * @brief Thread-safe allocator instance with synchronization
 */
typedef struct {
    char *heap;                /**< Heap memory pool */
    Block *head;               /**< Head of block list */
    pthread_mutex_t lock;      /**< Mutex for thread safety */
    AllocatorStats stats;      /**< Current statistics */
} Allocator;

/* Function Prototypes */

/**
 * Initialize the allocator with thread safety
 * @return Pointer to allocator instance
 */
Allocator *allocator_init(void);

/**
 * Allocate memory with specified strategy
 * @param allocator Allocator instance
 * @param size Number of bytes to allocate
 * @return Pointer to allocated memory, NULL on failure
 */
void *allocator_malloc(Allocator *allocator, size_t size);

/**
 * Free previously allocated memory with coalescing
 * @param allocator Allocator instance
 * @param ptr Pointer to memory to free
 */
void allocator_free(Allocator *allocator, void *ptr);

/**
 * Reallocate memory to new size
 * @param allocator Allocator instance
 * @param ptr Current memory pointer
 * @param size New size
 * @return Pointer to reallocated memory
 */
void *allocator_realloc(Allocator *allocator, void *ptr, size_t size);

/**
 * Print heap status for visualization
 * @param allocator Allocator instance
 */
void print_heap_status(Allocator *allocator);

/**
 * Get current allocator statistics
 * @param allocator Allocator instance
 * @return Statistics structure
 */
AllocatorStats get_allocator_stats(Allocator *allocator);

/**
 * Reset allocator state
 * @param allocator Allocator instance
 */
void allocator_reset(Allocator *allocator);

/**
 * Destroy allocator and free resources
 * @param allocator Allocator instance
 */
void allocator_destroy(Allocator *allocator);

/**
 * Verify heap integrity
 * @param allocator Allocator instance
 * @return true if heap is valid
 */
bool allocator_verify(Allocator *allocator);

/**
 * Export heap state as JSON string
 * @param allocator Allocator instance
 * @return JSON string (must be freed by caller)
 */
char *export_heap_state_json(Allocator *allocator);

/**
 * Free JSON string created by export_heap_state_json
 * @param json JSON string to free
 */
void free_json_state(char *json);

/* Strategy Selection */
#ifndef USE_FIRST_FIT
#define USE_BEST_FIT 1
#endif

#endif /* CUSTOM_ALLOCATOR_H */
