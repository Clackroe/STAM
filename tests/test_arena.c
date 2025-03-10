#define ARENA_IMPLEMENTATION
#include "../Arena.h"
#include <stdio.h>

int main()
{
    printf("Testing C Arena Implementation\n");

    Arena* arena = create_arena(1 KB);

    if (!arena) {
        printf("Failed to create arena\n");
        return 1;
    }

    int* a = (int*)arena_allocate(arena, sizeof(int));
    if (a) {
        *a = 42;
        printf("Allocated int: %d\n", *a);
    } else {
        printf("Failed to allocate int\n");
    }

    double* b = (double*)arena_allocate(arena, sizeof(double));
    if (b) {
        *b = 3.14;
        printf("Allocated double: %f\n", *b);
    } else {
        printf("Failed to allocate double\n");
    }

    print_arena(arena);

    arena_reset(arena);
    printf("Arena reset.\n");
    print_arena(arena);

    arena_free(arena);
    printf("Arena freed.\n");

    return 0;
}
