#define ARENA_IMPLEMENTATION
#include "../Arena.h"
#include <stdio.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Utility for timing
double get_time_sec()
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec + (double)ts.tv_nsec / 1.0e9;
}

// Test structure with varying sizes
typedef struct {
    int id;
    char data[64]; // Fixed-size array for consistent testing
} TestObject;

// Function to validate memory by writing and reading patterns
int validate_memory(void* ptr, size_t size)
{
    unsigned char* mem = (unsigned char*)ptr;

    // Write a pattern
    for (size_t i = 0; i < size; i++) {
        mem[i] = (unsigned char)(i % 256);
    }

    // Read and verify
    for (size_t i = 0; i < size; i++) {
        if (mem[i] != (unsigned char)(i % 256)) {
            printf("Memory validation failed at offset %zu\n", i);
            return 0;
        }
    }

    return 1;
}

// Test basic allocation
void test_basic_allocation()
{
    printf("\n=== BASIC ALLOCATION TEST ===\n");

    // Create arena
    Arena* arena = create_arena(1 MB);

    // Allocate and validate
    void* ptr1 = arena_allocate(arena, 1024);
    void* ptr2 = arena_allocate(arena, 2048);
    void* ptr3 = arena_allocate(arena, 4096);

    if (!ptr1 || !ptr2 || !ptr3) {
        printf("Basic allocation test failed: One or more allocations returned NULL\n");
    } else {
        int valid = validate_memory(ptr1, 1024) && validate_memory(ptr2, 2048) && validate_memory(ptr3, 4096);

        printf("Basic allocation test %s\n", valid ? "PASSED" : "FAILED");
    }

    print_arena(arena);
    arena_free(arena);
}

// Test region allocation directly
void test_region_allocation()
{
    printf("\n=== REGION ALLOCATION TEST ===\n");

    Region* region = create_region(64 KB);
    if (!region) {
        printf("Failed to create region\n");
        return;
    }

    // Allocate until almost full
    void* ptrs[100];
    int count = 0;

    for (int i = 0; i < 100; i++) {
        ptrs[i] = region_allocate(region, 500); // 500 bytes each
        if (!ptrs[i]) {
            break;
        }
        if (!validate_memory(ptrs[i], 500)) {
            printf("Region memory validation failed\n");
            break;
        }
        count++;
    }

    printf("Successfully allocated %d objects in region\n", count);
    print_region(region);

    // Test reset
    region_reset(region);
    printf("After reset:\n");
    print_region(region);

    // Test allocation after reset
    void* new_ptr = region_allocate(region, 1000);
    if (new_ptr && validate_memory(new_ptr, 1000)) {
        printf("Region allocation after reset: PASSED\n");
    } else {
        printf("Region allocation after reset: FAILED\n");
    }

    region_free(region);
}

// Test arena scratch functionality
void test_arena_scratch()
{
    printf("\n=== ARENA SCRATCH TEST ===\n");

    Arena* arena = create_arena(1 MB);

    // Allocate some memory
    void* ptr1 = arena_allocate(arena, 1024);
    void* ptr2 = arena_allocate(arena, 2048);

    if (!ptr1 || !ptr2) {
        printf("Initial allocations failed\n");
        arena_free(arena);
        return;
    }

    printf("Initial state:\n");
    print_arena(arena);

    // Create a scratch mark
    ArenaMark mark = arena_scratch(arena);

    // Allocate more memory
    void* ptr3 = arena_allocate(arena, 4096);
    void* ptr4 = arena_allocate(arena, 8192);

    printf("After additional allocations:\n");
    print_arena(arena);

    // Pop the scratch mark
    arena_pop_scratch(arena, mark);

    printf("After popping scratch mark:\n");
    print_arena(arena);

    // Allocate again to verify functionality
    void* ptr5 = arena_allocate(arena, 3000);
    if (ptr5 && validate_memory(ptr5, 3000)) {
        printf("Allocation after pop scratch: PASSED\n");
    } else {
        printf("Allocation after pop scratch: FAILED\n");
    }

    arena_free(arena);
}

// Test free list functionality
void test_free_list()
{
    printf("\n=== FREE LIST TEST ===\n");

    Arena* arena = create_freelist_arena(1 MB);

    // Allocate a bunch of objects
    void* ptrs[10];
    for (int i = 0; i < 10; i++) {
        ptrs[i] = arena_allocate(arena, 1024);
        if (!ptrs[i]) {
            printf("Free list allocation %d failed\n", i);
            arena_free(arena);
            return;
        }
        memset(ptrs[i], i + 1, 1024); // Fill with a pattern
    }

    printf("After initial allocations:\n");
    print_arena(arena);

    // Free some objects in mixed order
    arena_add_free_list(arena, ptrs[3], 1024);
    arena_add_free_list(arena, ptrs[5], 1024);
    arena_add_free_list(arena, ptrs[7], 1024);

    // Try to allocate again - should reuse freed blocks
    void* new_ptr1 = arena_allocate(arena, 1024);
    void* new_ptr2 = arena_allocate(arena, 1024);
    void* new_ptr3 = arena_allocate(arena, 1024);

    // Check if we got one of the freed pointers
    int reused = 0;
    for (int i = 0; i < 3; i++) {
        void* check_ptr = (i == 0) ? new_ptr1 : ((i == 1) ? new_ptr2 : new_ptr3);
        if (check_ptr == ptrs[3] || check_ptr == ptrs[5] || check_ptr == ptrs[7]) {
            reused++;
        }
    }

    printf("Reused %d/3 free list pointers\n", reused);
    printf("Free list test %s\n", (reused > 0) ? "PASSED" : "FAILED");

    // Clean up
    arena_free(arena);
}

// Performance test: Compare arena vs malloc
void performance_test(int num_allocations, int min_size, int max_size)
{
    printf("\n=== PERFORMANCE TEST ===\n");
    printf("Allocations: %d, Size range: %d - %d bytes\n",
        num_allocations, min_size, max_size);

    // Prepare allocation sizes
    int* sizes = (int*)malloc(num_allocations * sizeof(int));
    if (!sizes) {
        printf("Failed to allocate sizes array\n");
        return;
    }

    // Generate random sizes
    srand(42); // Fixed seed for reproducibility
    for (int i = 0; i < num_allocations; i++) {
        sizes[i] = min_size + rand() % (max_size - min_size + 1);
    }

    // Test malloc
    void** malloc_ptrs = (void**)malloc(num_allocations * sizeof(void*));
    if (!malloc_ptrs) {
        printf("Failed to allocate malloc_ptrs array\n");
        free(sizes);
        return;
    }

    double malloc_start = get_time_sec();

    for (int i = 0; i < num_allocations; i++) {
        malloc_ptrs[i] = malloc(sizes[i]);
        if (!malloc_ptrs[i]) {
            printf("Malloc failed at allocation %d\n", i);
            break;
        }
        // Touch the memory to ensure fair comparison
        memset(malloc_ptrs[i], i % 256, sizes[i]);
    }

    double malloc_end = get_time_sec();
    double malloc_time = malloc_end - malloc_start;

    // Free malloc memory
    malloc_start = get_time_sec();
    for (int i = 0; i < num_allocations; i++) {
        if (malloc_ptrs[i]) {
            free(malloc_ptrs[i]);
        }
    }
    malloc_end = get_time_sec();
    double malloc_free_time = malloc_end - malloc_start;

    // Test arena allocator
    Arena* arena = create_arena(10 MB);
    if (!arena) {
        printf("Failed to create arena\n");
        free(malloc_ptrs);
        free(sizes);
        return;
    }

    void** arena_ptrs = (void**)malloc(num_allocations * sizeof(void*));
    if (!arena_ptrs) {
        printf("Failed to allocate arena_ptrs array\n");
        arena_free(arena);
        free(malloc_ptrs);
        free(sizes);
        return;
    }

    double arena_start = get_time_sec();

    for (int i = 0; i < num_allocations; i++) {
        arena_ptrs[i] = arena_allocate(arena, sizes[i]);
        if (!arena_ptrs[i]) {
            printf("Arena allocation failed at allocation %d\n", i);
            break;
        }
        // Touch the memory to ensure fair comparison
        memset(arena_ptrs[i], i % 256, sizes[i]);
    }

    double arena_end = get_time_sec();
    double arena_time = arena_end - arena_start;

    // Free arena memory
    arena_start = get_time_sec();
    arena_reset(arena);
    arena_end = get_time_sec();
    double arena_free_time = arena_end - arena_start;

    // Test freelist arena
    Arena* fl_arena = create_freelist_arena(10 MB);
    if (!fl_arena) {
        printf("Failed to create freelist arena\n");
        free(arena_ptrs);
        arena_free(arena);
        free(malloc_ptrs);
        free(sizes);
        return;
    }

    double fl_arena_start = get_time_sec();

    for (int i = 0; i < num_allocations; i++) {
        arena_ptrs[i] = arena_allocate(fl_arena, sizes[i]);
        if (!arena_ptrs[i]) {
            printf("Freelist arena allocation failed at allocation %d\n", i);
            break;
        }
        // Touch the memory to ensure fair comparison
        memset(arena_ptrs[i], i % 256, sizes[i]);
    }

    double fl_arena_end = get_time_sec();
    double fl_arena_time = fl_arena_end - fl_arena_start;

    // Print results
    printf("\nResults:\n");
    printf("malloc:         %.6f seconds (alloc) + %.6f seconds (free) = %.6f seconds total\n",
        malloc_time, malloc_free_time, malloc_time + malloc_free_time);
    printf("arena:          %.6f seconds (alloc) + %.6f seconds (reset) = %.6f seconds total\n",
        arena_time, arena_free_time, arena_time + arena_free_time);
    printf("freelist arena: %.6f seconds (alloc)\n", fl_arena_time);

    printf("\nPerformance comparison:\n");
    printf("arena vs malloc: %.2fx faster allocation, %.2fx faster total\n",
        malloc_time / arena_time, (malloc_time + malloc_free_time) / (arena_time + arena_free_time));

    // Cleanup
    arena_free(arena);
    arena_free(fl_arena);
    free(arena_ptrs);
    free(malloc_ptrs);
    free(sizes);
}

// Test allocation patterns
void test_allocation_patterns()
{
    printf("\n=== ALLOCATION PATTERNS TEST ===\n");

    Arena* arena = create_arena(2 MB);

    // 1. Sequential allocations
    printf("1. Sequential allocations test\n");
    int num_sequential = 1000;
    for (int i = 0; i < num_sequential; i++) {
        void* ptr = arena_allocate(arena, 128);
        if (!ptr) {
            printf("Sequential allocation failed at iteration %d\n", i);
            break;
        }
    }
    print_arena(arena);
    arena_reset(arena);

    // 2. Growing allocations
    printf("\n2. Growing allocations test\n");
    for (int i = 1; i <= 20; i++) {
        void* ptr = arena_allocate(arena, i * 1024); // 1KB to 20KB
        if (!ptr) {
            printf("Growing allocation failed at iteration %d (size: %d)\n", i, i * 1024);
            break;
        }
    }
    print_arena(arena);
    arena_reset(arena);

    // 3. Large allocation
    printf("\n3. Large allocation test\n");
    void* large_ptr = arena_allocate(arena, 1 MB);
    if (!large_ptr) {
        printf("Large allocation failed\n");
    } else {
        printf("Large allocation succeeded\n");
        if (validate_memory(large_ptr, 1 MB)) {
            printf("Large allocation memory validation: PASSED\n");
        } else {
            printf("Large allocation memory validation: FAILED\n");
        }
    }
    print_arena(arena);

    arena_free(arena);
}

// Test capacity overflow
void test_capacity_overflow()
{
    printf("\n=== CAPACITY OVERFLOW TEST ===\n");

    // Create a small arena
    Arena* arena = create_arena(4 KB);

    printf("Testing automatic region expansion:\n");

    // Try to allocate more than initial capacity
    void* ptr1 = arena_allocate(arena, 2 KB);
    void* ptr2 = arena_allocate(arena, 3 KB); // Should force a new region
    void* ptr3 = arena_allocate(arena, 4 KB); // Should force another region

    if (ptr1 && ptr2 && ptr3) {
        printf("Arena expansion test PASSED\n");
    } else {
        printf("Arena expansion test FAILED\n");
    }

    print_arena(arena);
    arena_free(arena);

    // Test region overflow directly
    printf("\nTesting region overflow handling:\n");
    Region* region = create_region(2 KB);

    // This should succeed
    void* r_ptr1 = region_allocate(region, 1 KB);
    if (r_ptr1) {
        printf("First region allocation: PASSED\n");
    } else {
        printf("First region allocation: FAILED\n");
    }

    // This should succeed too
    void* r_ptr2 = region_allocate(region, 500);
    if (r_ptr2) {
        printf("Second region allocation: PASSED\n");
    } else {
        printf("Second region allocation: FAILED\n");
    }

    // This should fail (exceeds capacity)
    void* r_ptr3 = region_allocate(region, 1 KB);
    if (!r_ptr3) {
        printf("Overflow detection: PASSED\n");
    } else {
        printf("Overflow detection: FAILED\n");
    }

    region_free(region);
}

// Test struct allocation and alignment
void test_struct_allocation()
{
    printf("\n=== STRUCT ALLOCATION TEST ===\n");

    Arena* arena = create_arena(1 MB);

    // Allocate different structs
    typedef struct {
        char c;
        int i;
        double d;
    } StructA;

    typedef struct {
        double d;
        char c;
        int i;
    } StructB;

    // Test alignment handling
    StructA* a1 = (StructA*)arena_allocate(arena, sizeof(StructA));
    StructB* b1 = (StructB*)arena_allocate(arena, sizeof(StructB));
    StructA* a2 = (StructA*)arena_allocate(arena, sizeof(StructA));
    StructB* b2 = (StructB*)arena_allocate(arena, sizeof(StructB));

    // Initialize structs
    if (a1 && b1 && a2 && b2) {
        a1->c = 'A';
        a1->i = 123;
        a1->d = 3.14;

        b1->d = 2.71;
        b1->c = 'B';
        b1->i = 456;

        a2->c = 'C';
        a2->i = 789;
        a2->d = 1.41;

        b2->d = 1.73;
        b2->c = 'D';
        b2->i = 101;

        // Verify structs
        int passed = (a1->c == 'A' && a1->i == 123 && a1->d == 3.14 && b1->d == 2.71 && b1->c == 'B' && b1->i == 456 && a2->c == 'C' && a2->i == 789 && a2->d == 1.41 && b2->d == 1.73 && b2->c == 'D' && b2->i == 101);

        printf("Struct allocation and alignment test: %s\n", passed ? "PASSED" : "FAILED");

        // Print alignment info
        printf("StructA alignment check: %s\n",
            ((uintptr_t)a1 % sizeof(uintptr_t) == 0 && (uintptr_t)a2 % sizeof(uintptr_t) == 0) ? "PASSED" : "FAILED");
        printf("StructB alignment check: %s\n",
            ((uintptr_t)b1 % sizeof(uintptr_t) == 0 && (uintptr_t)b2 % sizeof(uintptr_t) == 0) ? "PASSED" : "FAILED");
    } else {
        printf("Struct allocation failed\n");
    }

    arena_free(arena);
}

// Test mixed allocation and freelist
void test_mixed_allocations()
{
    printf("\n=== MIXED ALLOCATION TEST ===\n");

    Arena* arena = create_freelist_arena(1 MB);

    // Allocate 100 blocks of various sizes
    void* ptrs[100];
    int sizes[100];

    for (int i = 0; i < 100; i++) {
        // Generate random size between 32 and 2048 bytes
        sizes[i] = 32 + (rand() % 2016);
        ptrs[i] = arena_allocate(arena, sizes[i]);

        if (!ptrs[i]) {
            printf("Mixed allocation failed at index %d\n", i);
            break;
        }

        // Write a pattern to memory
        memset(ptrs[i], i % 256, sizes[i]);
    }

    printf("Initial mixed allocations completed\n");
    print_arena(arena);

    // Free some random blocks
    int free_count = 0;
    for (int i = 0; i < 100; i += 3) {
        arena_add_free_list(arena, ptrs[i], sizes[i]);
        free_count++;
    }

    printf("Freed %d blocks\n", free_count);

    // Allocate some more blocks
    int realloc_count = 0;
    for (int i = 0; i < 30; i++) {
        int size = 32 + (rand() % 2016);
        void* ptr = arena_allocate(arena, size);

        if (!ptr) {
            printf("Reallocation failed at index %d\n", i);
            break;
        }

        // Write a pattern to memory
        memset(ptr, 100 + i, size);
        realloc_count++;
    }

    printf("Reallocated %d blocks\n", realloc_count);
    print_arena(arena);

    arena_free(arena);
}

// Test stress case with many small allocations
void test_stress_small_allocations()
{
    printf("\n=== STRESS TEST: SMALL ALLOCATIONS ===\n");

    // Time malloc
    int num_allocations = 100000;
    int alloc_size = 32;

    double malloc_start = get_time_sec();
    void** malloc_ptrs = (void**)malloc(num_allocations * sizeof(void*));

    for (int i = 0; i < num_allocations; i++) {
        malloc_ptrs[i] = malloc(alloc_size);
        if (malloc_ptrs[i]) {
            *(int*)malloc_ptrs[i] = i; // Simple write
        }
    }

    // Free malloc memory
    for (int i = 0; i < num_allocations; i++) {
        if (malloc_ptrs[i]) {
            free(malloc_ptrs[i]);
        }
    }
    free(malloc_ptrs);

    double malloc_end = get_time_sec();
    double malloc_time = malloc_end - malloc_start;

    // Time arena
    Arena* arena = create_arena(10 MB);

    double arena_start = get_time_sec();
    for (int i = 0; i < num_allocations; i++) {
        void* ptr = arena_allocate(arena, alloc_size);
        if (ptr) {
            *(int*)ptr = i; // Simple write
        }
    }
    arena_free(arena);
    double arena_end = get_time_sec();
    double arena_time = arena_end - arena_start;

    printf("Small allocations stress test results:\n");
    printf("malloc: %.6f seconds\n", malloc_time);
    printf("arena:  %.6f seconds\n", arena_time);
    printf("Speed ratio: %.2fx\n", malloc_time / arena_time);
}

int main()
{
    printf("=== MEMORY ARENA TEST SUITE ===\n");

    // Basic tests
    test_basic_allocation();
    test_region_allocation();
    test_arena_scratch();
    test_free_list();

    // Advanced tests
    test_allocation_patterns();
    test_capacity_overflow();
    test_struct_allocation();
    test_mixed_allocations();

    // Performance tests
    test_stress_small_allocations();

    // Comprehensive performance test
    performance_test(10000, 16, 4096); // Many small-medium allocations
    performance_test(100, 1 KB, 1 MB); // Fewer large allocations
    performance_test(1000, 64, 16 KB); // Mixed allocation sizes

    printf("\n=== TEST SUITE COMPLETE ===\n");

    return 0;
}

//
// int main()
// {
//     printf("Testing C Arena Implementation\n");
//
//     Arena* arena = create_arena(1 KB);
//
//     if (!arena) {
//         printf("Failed to create arena\n");
//         return 1;
//     }
//
//     int* a = (int*)arena_allocate(arena, sizeof(int));
//     if (a) {
//         *a = 42;
//         printf("Allocated int: %d\n", *a);
//     } else {
//         printf("Failed to allocate int\n");
//     }
//
//     double* b = (double*)arena_allocate(arena, sizeof(double));
//     if (b) {
//         *b = 3.14;
//         printf("Allocated double: %f\n", *b);
//     } else {
//         printf("Failed to allocate double\n");
//     }
//
//     print_arena(arena);
//
//     arena_reset(arena);
//     printf("Arena reset.\n");
//     print_arena(arena);
//
//     arena_free(arena);
//     printf("Arena freed.\n");
//
//     return 0;
// }
