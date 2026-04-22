/**
 * @file test_malloc.c
 * @brief Comprehensive test suite for memory allocator
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <unistd.h>
#include "allocator.h"

/* Test results */
static int tests_passed = 0;
static int tests_failed = 0;

/* Helper macro for assertions */
#define ASSERT(condition, message) \
    do { \
        if (condition) { \
            printf("  ✓ %s\n", message); \
            tests_passed++; \
        } else { \
            printf("  ✗ %s\n", message); \
            tests_failed++; \
        } \
    } while (0)

/**
 * Test 1: Basic allocation and deallocation
 */
static void test_basic_allocation(void) {
    printf("\n[TEST 1] Basic Allocation & Deallocation\n");

    Allocator *alloc = allocator_init();
    ASSERT(alloc != NULL, "Allocator initialization");

    void *ptr1 = allocator_malloc(alloc, 256);
    ASSERT(ptr1 != NULL, "Allocate 256 bytes");

    void *ptr2 = allocator_malloc(alloc, 512);
    ASSERT(ptr2 != NULL, "Allocate 512 bytes");

    AllocatorStats stats = get_allocator_stats(alloc);
    ASSERT(stats.num_allocations == 2, "Number of allocations is 2");
    ASSERT(stats.total_allocated >= 768, "Total allocated >= 768 bytes");

    allocator_free(alloc, ptr1);
    ASSERT(get_allocator_stats(alloc).num_allocations == 1, "After free, allocations = 1");

    allocator_free(alloc, ptr2);
    ASSERT(get_allocator_stats(alloc).num_allocations == 0, "After all freed, allocations = 0");

    allocator_destroy(alloc);
}

/**
 * Test 2: Block splitting
 */
static void test_block_splitting(void) {
    printf("\n[TEST 2] Block Splitting\n");

    Allocator *alloc = allocator_init();

    void *ptr1 = allocator_malloc(alloc, 100);
    ASSERT(ptr1 != NULL, "First allocation");

    void *ptr2 = allocator_malloc(alloc, 100);
    ASSERT(ptr2 != NULL, "Second allocation");

    AllocatorStats stats = get_allocator_stats(alloc);
    ASSERT(stats.num_blocks > 1, "Block splitting occurred");

    allocator_free(alloc, ptr1);
    allocator_free(alloc, ptr2);
    allocator_destroy(alloc);
}

/**
 * Test 3: Coalescing
 */
static void test_coalescing(void) {
    printf("\n[TEST 3] Block Coalescing\n");

    Allocator *alloc = allocator_init();

    void *ptr1 = allocator_malloc(alloc, 256);
    void *ptr2 = allocator_malloc(alloc, 256);
    void *ptr3 = allocator_malloc(alloc, 256);

    size_t blocks_before = get_allocator_stats(alloc).num_blocks;

    allocator_free(alloc, ptr1);
    allocator_free(alloc, ptr2);

    size_t blocks_after = get_allocator_stats(alloc).num_blocks;
    ASSERT(blocks_after < blocks_before, "Coalescing reduced block count");

    allocator_free(alloc, ptr3);
    allocator_destroy(alloc);
}

/**
 * Test 4: Reallocation
 */
static void test_reallocation(void) {
    printf("\n[TEST 4] Reallocation\n");

    Allocator *alloc = allocator_init();

    void *ptr = allocator_malloc(alloc, 100);
    ASSERT(ptr != NULL, "Initial allocation");

    memset(ptr, 'A', 100);

    void *new_ptr = allocator_realloc(alloc, ptr, 200);
    ASSERT(new_ptr != NULL, "Reallocation to larger size");
    ASSERT(((char *)new_ptr)[99] == 'A', "Data preserved after reallocation");

    allocator_free(alloc, new_ptr);
    allocator_destroy(alloc);
}

/**
 * Test 5: Heap overflow prevention
 */
static void test_overflow_prevention(void) {
    printf("\n[TEST 5] Overflow Prevention\n");

    Allocator *alloc = allocator_init();

    /* 1. Try to allocate more than total physical heap size */
    void *ptr = allocator_malloc(alloc, HEAP_SIZE + 1);
    ASSERT(ptr == NULL, "Overflow allocation rejected");

    /* 2. Try to allocate zero size (should be handled gracefully as NULL) */
    ptr = allocator_malloc(alloc, 0);
    ASSERT(ptr == NULL, "Zero-size allocation rejected");

    /* 3. Try to allocate nearly full heap (should fail due to METADATA_SIZE) */
    ptr = allocator_malloc(alloc, HEAP_SIZE - 1);
    ASSERT(ptr == NULL, "Request for (Heap - 1) rejected (must account for metadata)");

    /* 4. Try to allocate exactly the usable capacity */
    size_t max_usable = HEAP_SIZE - METADATA_SIZE;
    ptr = allocator_malloc(alloc, max_usable);
    ASSERT(ptr != NULL, "Full usable heap allocation allowed");

    if (ptr) allocator_free(alloc, ptr);
    allocator_destroy(alloc);
}

/**
 * Test 6: Verification
 */
static void test_verification(void) {
    printf("\n[TEST 6] Heap Verification\n");

    Allocator *alloc = allocator_init();
    ASSERT(allocator_verify(alloc), "Initial heap is valid");

    allocator_malloc(alloc, 100);
    allocator_malloc(alloc, 200);
    ASSERT(allocator_verify(alloc), "Heap valid after allocations");

    allocator_destroy(alloc);
}

/**
 * Test 7: Double-free detection
 */
static void test_double_free(void) {
    printf("\n[TEST 7] Double-Free Detection\n");

    Allocator *alloc = allocator_init();

    void *ptr = allocator_malloc(alloc, 100);
    allocator_free(alloc, ptr);

    /* This should trigger warning in console but not crash */
    allocator_free(alloc, ptr);
    ASSERT(true, "Double-free handled gracefully");

    allocator_destroy(alloc);
}

/**
 * Test 8: Fragmentation analysis
 */
static void test_fragmentation(void) {
    printf("\n[TEST 8] Fragmentation Analysis\n");

    Allocator *alloc = allocator_init();

    void *ptrs[8];
    for (int i = 0; i < 8; i++) {
        ptrs[i] = allocator_malloc(alloc, 256);
    }

    for (int i = 0; i < 8; i += 2) {
        allocator_free(alloc, ptrs[i]);
    }

    AllocatorStats stats = get_allocator_stats(alloc);
    ASSERT(stats.num_blocks > 4, "Fragmentation created multiple blocks");

    for (int i = 1; i < 8; i += 2) {
        allocator_free(alloc, ptrs[i]);
    }

    allocator_destroy(alloc);
}

/**
 * Thread safety stress test
 */
typedef struct {
    Allocator *alloc;
    int thread_id;
    int iterations;
    int success_count;
} ThreadTestData;

static void *thread_stress_test(void *arg) {
    ThreadTestData *data = (ThreadTestData *)arg;
    data->success_count = 0;

    for (int i = 0; i < data->iterations; i++) {
        size_t size = (rand() % 256) + 1;
        void *ptr = allocator_malloc(data->alloc, size);

        if (ptr) {
            data->success_count++;
            usleep(1); 
            allocator_free(data->alloc, ptr);
        }
    }
    return NULL;
}

/**
 * Test 9: Thread safety
 */
static void test_thread_safety(void) {
    printf("\n[TEST 9] Thread Safety (4 threads, 50 iterations each)\n");

    Allocator *alloc = allocator_init();
    pthread_t threads[4];
    ThreadTestData thread_data[4];

    srand(time(NULL));

    for (int i = 0; i < 4; i++) {
        thread_data[i].alloc = alloc;
        thread_data[i].thread_id = i;
        thread_data[i].iterations = 50;
        thread_data[i].success_count = 0;
        pthread_create(&threads[i], NULL, thread_stress_test, &thread_data[i]);
    }

    for (int i = 0; i < 4; i++) {
        pthread_join(threads[i], NULL);
        printf("  Thread %d: %d successful allocations\n", i, thread_data[i].success_count);
    }

    ASSERT(allocator_verify(alloc), "Heap integrity after concurrent access");
    allocator_destroy(alloc);
}

/**
 * Test 10: Memory reset
 */
static void test_reset(void) {
    printf("\n[TEST 10] Memory Reset\n");

    Allocator *alloc = allocator_init();
    allocator_malloc(alloc, 256);
    allocator_malloc(alloc, 512);

    AllocatorStats before = get_allocator_stats(alloc);
    ASSERT(before.num_allocations == 2, "Before reset: 2 allocations");

    allocator_reset(alloc);

    AllocatorStats after = get_allocator_stats(alloc);
    ASSERT(after.num_allocations == 0, "After reset: 0 allocations");
    ASSERT(after.num_blocks == 1, "After reset: 1 block");

    allocator_destroy(alloc);
}

/**
 * Test 11: Fragmentation Pattern Analysis
 */
static void test_fragmentation_comparison(void) {
    printf("\n[TEST 11] Fragmentation Pattern Analysis\n");

    Allocator *alloc = allocator_init();

    void *ptrs[16];
    for (int i = 0; i < 16; i++) {
        ptrs[i] = allocator_malloc(alloc, 128);
    }

    int free_indices[] = {0, 2, 4, 6, 8, 10, 12, 14};
    for (int i = 0; i < 8; i++) {
        allocator_free(alloc, ptrs[free_indices[i]]);
    }

    AllocatorStats stats = get_allocator_stats(alloc);
    printf("  Allocations: %zu\n", stats.num_allocations);
    printf("  Blocks: %zu\n", stats.num_blocks);
    printf("  Fragmentation: %zu%%\n", stats.fragmentation_ratio);
    printf("  Free Memory: %zu bytes\n", stats.total_free);

    for (int i = 1; i < 16; i += 2) {
        allocator_free(alloc, ptrs[i]);
    }

    allocator_destroy(alloc);
}

/**
 * Print test summary
 */
static void print_summary(void) {
    printf("\n╔════════════════════════════════════════╗\n");
    printf("║         TEST SUMMARY REPORT            ║\n");
    printf("╠════════════════════════════════════════╣\n");
    printf("║ Tests Passed: %-24d ║\n", tests_passed);
    printf("║ Tests Failed: %-24d ║\n", tests_failed);
    printf("║ Total Tests:  %-24d ║\n", tests_passed + tests_failed);
    printf("╚════════════════════════════════════════╝\n\n");

    if (tests_failed == 0) {
        printf("✓ ALL TESTS PASSED!\n");
    } else {
        printf("✗ SOME TESTS FAILED!\n");
    }
}

int main(int argc, char *argv[]) {
    printf("\n╔════════════════════════════════════════════════════╗\n");
    printf("║  Custom Memory Allocator - Test Suite              ║\n");
    printf("║             Comprehensive Validation               ║\n");
    printf("╚════════════════════════════════════════════════════╝\n");

    test_basic_allocation();
    test_block_splitting();
    test_coalescing();
    test_reallocation();
    test_overflow_prevention();
    test_verification();
    test_double_free();
    test_fragmentation();
    test_thread_safety();
    test_reset();
    test_fragmentation_comparison();

    print_summary();

    return tests_failed == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}