#define ARENA_IMPLEMENTATION
#include "Arena.h"

#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Allocate and fill an array of integers
void test_int_array(Arena* arena, size_t count)
{
    printf("\n=== Testing allocation of %zu integers ===\n", count);
    int* numbers = arena_allocate(arena, count * sizeof(int));
    if (!numbers) {
        printf("Failed to allocate int array\n");
        return;
    }

    // Fill with incrementing values
    for (size_t i = 0; i < count; i++) {
        numbers[i] = (int)i;
    }

    // Verify a few random elements
    for (size_t i = 0; i < 5 && i < count; i++) {
        size_t idx = i * count / 5;
        printf("numbers[%zu] = %d\n", idx, numbers[idx]);
    }
    print_arena(arena);
}

// Allocate and fill an array of structs
typedef struct {
    double x, y, z;
    char name[32];
    uint64_t id;
} Vector3D;

void test_struct_array(Arena* arena, size_t count)
{
    printf("\n=== Testing allocation of %zu Vector3D structs ===\n", count);
    Vector3D* vectors = arena_allocate(arena, count * sizeof(Vector3D));
    if (!vectors) {
        printf("Failed to allocate Vector3D array\n");
        return;
    }

    // Fill with data
    for (size_t i = 0; i < count; i++) {
        vectors[i].x = (double)i * 1.1;
        vectors[i].y = (double)i * 2.2;
        vectors[i].z = (double)i * 3.3;
        vectors[i].id = i + 1000;
        snprintf(vectors[i].name, 32, "Vector%zu", i);
    }

    // Verify a few random elements
    for (size_t i = 0; i < 3 && i < count; i++) {
        size_t idx = i * count / 3;
        printf("vectors[%zu] = {%f, %f, %f, %s, %" PRIu64 "}\n",
            idx, vectors[idx].x, vectors[idx].y, vectors[idx].z,
            vectors[idx].name, vectors[idx].id);
    }
    print_arena(arena);
}

// Test mixed allocation patterns
void test_mixed_allocations(Arena* arena, size_t iterations)
{
    printf("\n=== Testing %zu mixed allocations ===\n", iterations);

    // Array to store pointers for validation
    void** pointers = malloc(iterations * sizeof(void*));
    size_t* sizes = malloc(iterations * sizeof(size_t));

    // Perform random allocations of different sizes
    srand((unsigned int)time(NULL));
    for (size_t i = 0; i < iterations; i++) {
        // Generate random size between 8 bytes and 1 KB
        size_t size = 8 + (rand() % 1016);
        sizes[i] = size;

        // Allocate memory
        unsigned char* memory = arena_allocate(arena, size);
        pointers[i] = memory;

        if (!memory) {
            printf("Failed to allocate %zu bytes at iteration %zu\n", size, i);
            continue;
        }

        // Fill with a recognizable pattern based on iteration
        memset(memory, i % 256, size);
    }

    // Verify some random allocations
    for (size_t i = 0; i < 10 && i < iterations; i++) {
        size_t idx = rand() % iterations;
        unsigned char* memory = pointers[idx];

        if (!memory)
            continue;

        // Check first byte
        printf("Allocation %zu (size %zu): First byte = %u (expected %u)\n",
            idx, sizes[idx], memory[0], (unsigned int)(idx % 256));
    }

    free(pointers);
    free(sizes);
    print_arena(arena);
}

// Test edge cases
void test_edge_cases(Arena* arena)
{
    printf("\n=== Testing edge cases ===\n");

    // Test very small allocation
    char* small = arena_allocate(arena, 1);
    if (small) {
        *small = 'A';
        printf("Small allocation successful: %c\n", *small);
    }

    // Test zero-sized allocation (implementation-defined behavior)
    void* zero = arena_allocate(arena, 0);
    printf("Zero-sized allocation: %p\n", zero);

    // Test allocation that forces creation of a new region
    size_t large_size = 20 * 1024 * 1024; // 20 MB
    printf("Attempting large allocation of %zu bytes...\n", large_size);
    void* large = arena_allocate(arena, large_size);
    if (large) {
        printf("Large allocation successful\n");
        // Write to first and last byte to verify it's usable
        ((char*)large)[0] = 'X';
        ((char*)large)[large_size - 1] = 'Z';
        printf("First and last bytes: %c %c | Expected X, Z\n",
            ((char*)large)[0], ((char*)large)[large_size - 1]);
    }

    print_arena(arena);
}

// Performance test
void test_performance(uint32_t region_size)
{
    printf("\n=== Performance test with initial region size of %" PRIu32 " bytes ===\n", region_size);

    const int NUM_ALLOCS = 1000000;
    const int ALLOC_SIZE = 8; // 8 bytes each

    clock_t start = clock();

    // Create arena
    Arena* arena = create_arena(region_size);

    // Perform many small allocations
    for (int i = 0; i < NUM_ALLOCS; i++) {
        void* ptr = arena_allocate(arena, ALLOC_SIZE);
        if (!ptr) {
            printf("Allocation failed at iteration %d\n", i);
            break;
        }
    }

    clock_t end = clock();
    double time_spent = (double)(end - start) / CLOCKS_PER_SEC;

    printf("Time to allocate %d blocks of %d bytes: %.3f seconds\n",
        NUM_ALLOCS, ALLOC_SIZE, time_spent);
    printf("Allocations per second: %.0f\n", NUM_ALLOCS / time_spent);

    print_arena(arena);
    arena_free(arena);
}

// Test comparison with standard malloc
void compare_with_malloc()
{
    printf("\n=== Comparing with standard malloc ===\n");

    const int NUM_ALLOCS = 1000000;
    const int ALLOC_SIZE = 8; // 8 bytes each

    // Test with malloc
    clock_t malloc_start = clock();
    void** ptrs = malloc(NUM_ALLOCS * sizeof(void*));

    for (int i = 0; i < NUM_ALLOCS; i++) {
        ptrs[i] = malloc(ALLOC_SIZE);
    }

    clock_t malloc_end = clock();
    double malloc_time = (double)(malloc_end - malloc_start) / CLOCKS_PER_SEC;

    // Free malloc'd memory
    for (int i = 0; i < NUM_ALLOCS; i++) {
        free(ptrs[i]);
    }
    free(ptrs);

    // Test with arena
    clock_t arena_start = clock();
    Arena* arena = create_arena(1 MB);

    for (int i = 0; i < NUM_ALLOCS; i++) {
        arena_allocate(arena, ALLOC_SIZE);
    }

    clock_t arena_end = clock();
    double arena_time = (double)(arena_end - arena_start) / CLOCKS_PER_SEC;

    arena_free(arena);

    printf("Time for %d allocations with malloc: %.3f seconds (%.0f allocs/sec)\n",
        NUM_ALLOCS, malloc_time, NUM_ALLOCS / malloc_time);
    printf("Time for %d allocations with arena: %.3f seconds (%.0f allocs/sec)\n",
        NUM_ALLOCS, arena_time, NUM_ALLOCS / arena_time);
    printf("Arena is %.2fx %s than malloc\n",
        malloc_time > arena_time ? malloc_time / arena_time : arena_time / malloc_time,
        malloc_time > arena_time ? "faster" : "slower");
}

int do_tests()
{
    printf("=== Arena Allocator Stress Test ===\n");

    // Create arena with initial 1 MB capacity
    Arena* arena = create_arena(1 MB);
    if (!arena) {
        printf("Failed to create arena\n");
        return 1;
    }

    // Run various tests
    test_int_array(arena, 1000);
    test_struct_array(arena, 500);
    test_mixed_allocations(arena, 1000);
    test_edge_cases(arena);

    // Clean up
    arena_free(arena);

    // Performance tests with different region sizes
    test_performance(1 KB);
    test_performance(1 MB);
    test_performance(10 MB);

    // Compare with malloc
    compare_with_malloc();

    printf("\n=== All tests completed ===\n");
    return 0;
}

int main(void)
{
    struct Point {
        int a;
        int b;
        int c;
    };

    Arena* arr = create_arena(10 MB);
    struct Point* p = (struct Point*)arena_allocate(arr, sizeof(struct Point));
    p = p + 1;
    print_arena(arr);

    arena_free(arr);

    return do_tests();
}
