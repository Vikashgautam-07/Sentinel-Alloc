/**
 * @file utils.c
 * @brief Utility functions for allocator
 */

#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <math.h>
#include "allocator.h"

/**
 * Calculate fragmentation ratio
 */
static size_t calculate_fragmentation(Allocator *allocator) {
    if (!allocator || allocator->stats.num_blocks == 0) {
        return 0;
    }

    /* Fragmentation = (Number of free blocks / Total blocks) * 100 */
    size_t free_blocks = 0;
    for (Block *current = allocator->head; current != NULL; current = current->next) {
        if (current->is_free) free_blocks++;
    }

    return (size_t)((free_blocks * 100.0) / allocator->stats.num_blocks);
}

/**
 * Update statistics
 */
void update_allocator_stats(Allocator *allocator) {
    if (!allocator) return;

    pthread_mutex_lock(&allocator->lock);

    allocator->stats.fragmentation_ratio = calculate_fragmentation(allocator);

    pthread_mutex_unlock(&allocator->lock);
}

/**
 * Print detailed heap visualization
 */
void print_heap_visualization(Allocator *allocator) {
    if (!allocator) return;

    pthread_mutex_lock(&allocator->lock);

    printf("\n╔════════════════════════════════════════════════════════╗\n");
    printf("║         HEAP MEMORY VISUALIZATION (4KB Total)         ║\n");
    printf("╚════════════════════════════════════════════════════════╝\n");

    size_t total_drawn = 0;
    const int width = 50;
    const int max_height = HEAP_SIZE / 100;

    for (Block *current = allocator->head; current != NULL; current = current->next) {
        size_t block_width = (current->size * width) / HEAP_SIZE;
        if (block_width == 0 && current->size > 0) block_width = 1;

        char fill_char = current->is_free ? '.' : '#';
        printf("  ");
        for (size_t i = 0; i < block_width; i++) {
            printf("%c", fill_char);
        }
        printf(" [%zu bytes, %s]\n", current->size,
               current->is_free ? "FREE" : "ALLOC");

        total_drawn += block_width;
    }

    printf("\n  Legend: # = Allocated, . = Free\n");
    printf("  Total Memory Used: %zu / %d bytes\n\n",
           allocator->stats.total_allocated, HEAP_SIZE);

    pthread_mutex_unlock(&allocator->lock);
}

/**
 * Export heap state to string for API responses
 */
char *export_heap_state_json(Allocator *allocator) {
    if (!allocator) return NULL;

    pthread_mutex_lock(&allocator->lock);

    const size_t BUF_SIZE = 4096;
    char *buffer = malloc(BUF_SIZE);
    if (!buffer) {
        pthread_mutex_unlock(&allocator->lock);
        return NULL;
    }

    int pos = 0;
    // Note the added (BUF_SIZE - pos) argument in every call:
    pos += snprintf(buffer + pos, BUF_SIZE - pos, "{\"heap_status\": {");
    pos += snprintf(buffer + pos, BUF_SIZE - pos, "\"total_size\": %d, ", HEAP_SIZE);
    pos += snprintf(buffer + pos, BUF_SIZE - pos, "\"allocated\": %zu, ", allocator->stats.total_allocated);
    pos += snprintf(buffer + pos, BUF_SIZE - pos, "\"free\": %zu, ", allocator->stats.total_free);
    pos += snprintf(buffer + pos, BUF_SIZE - pos, "\"num_allocations\": %zu, ", allocator->stats.num_allocations);
    pos += snprintf(buffer + pos, BUF_SIZE - pos, "\"num_blocks\": %zu, ", allocator->stats.num_blocks);
    pos += snprintf(buffer + pos, BUF_SIZE - pos, "\"fragmentation\": %zu, ", allocator->stats.fragmentation_ratio);
    pos += snprintf(buffer + pos, BUF_SIZE - pos, "\"failed_allocations\": %zu", allocator->stats.failed_allocations);
    pos += snprintf(buffer + pos, BUF_SIZE - pos, "}, \"blocks\": [");

    bool first = true;
    for (Block *current = allocator->head; current != NULL; current = current->next) {
        if (!first) {
            pos += snprintf(buffer + pos, BUF_SIZE - pos, ", ");
        }
        pos += snprintf(buffer + pos, BUF_SIZE - pos, 
                        "{\"size\": %zu, \"is_free\": %s, \"address\": \"%p\"}",
                        current->size,
                        current->is_free ? "true" : "false",
                        (void *)current);
        first = false;
        
        // Safety check to prevent overflow
        if (pos >= (int)BUF_SIZE - 64) break; 
    }

    pos += snprintf(buffer + pos, BUF_SIZE - pos, "]}");

    pthread_mutex_unlock(&allocator->lock);
    return buffer;
}

/**
 * Free JSON export buffer
 */
void free_json_state(char *json) {
    if (json) free(json);
}

/**
 * Analyze memory access pattern
 */
void analyze_memory_pattern(Allocator *allocator) {
    if (!allocator) return;

    pthread_mutex_lock(&allocator->lock);

    printf("\n=== MEMORY ACCESS ANALYSIS ===\n");

    size_t largest_free = 0;
    size_t smallest_alloc = (size_t)-1;

    for (Block *current = allocator->head; current != NULL; current = current->next) {
        if (current->is_free && current->size > largest_free) {
            largest_free = current->size;
        }
        if (!current->is_free && current->size < smallest_alloc) {
            smallest_alloc = current->size;
        }
    }

    printf("Largest Free Block: %zu bytes\n", largest_free);
    printf("Smallest Allocated Block: %zu bytes\n",
           smallest_alloc == (size_t)-1 ? 0 : smallest_alloc);
    printf("Utilization: %.2f%%\n",
           (allocator->stats.total_allocated * 100.0) / HEAP_SIZE);
    printf("Fragmentation: %zu%%\n\n", allocator->stats.fragmentation_ratio);

    pthread_mutex_unlock(&allocator->lock);
}
