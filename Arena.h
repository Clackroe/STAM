#include <inttypes.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define KB *1024
#define MB *1024 * 1024
#define GB *1024 * 1024 * 1024

#define ALIGN_SIZE(size_bytes) (size_bytes + sizeof(uintptr_t) - 1) / sizeof(uintptr_t);

typedef struct Region {
    uint32_t data_count;
    uint32_t capacity;
    struct Region* next;
    uintptr_t data[];
} Region;

typedef struct Arena {
    Region* start;
    Region* end;
} Arena;

Region* create_region(uint32_t size_bytes);
void* region_allocate(Region* reg, uint32_t size_bytes);
void region_reset(Region* reg);
void region_free(Region* reg);
void print_region(Region* reg);

Arena* create_arena(uint32_t size_bytes);
void* arena_allocate(Arena* arena, uint32_t size_bytes);
void arena_reset(Arena* arena);
void arena_free(Arena* arena);
void print_arena(Arena* arena);

#ifdef ARENA_CPP

struct ArenaCPP {

    ArenaCPP(uint32_t size_bytes)
    {
        arena = create_arena(size_bytes);
    }
    ~ArenaCPP()
    {
        arena_free(arena);
    }

    template <typename T, typename... Args>
    T* allocate(Args... args)
    {
        void* obj = arena_allocate(arena, sizeof(T));
        return new (obj) T(args...);

    };


    void reset()
    {
        arena_reset(arena);
    }
    void print(){
        print_arena(arena);
    }

private:
    Arena* arena;
};

#endif // ARENA_CPP

#ifdef ARENA_IMPLEMENTATION

Region* create_region(uint32_t size_bytes)
{
    size_t size = ALIGN_SIZE(size_bytes);

    Region* region = (Region*)malloc(sizeof(Region) + size * sizeof(uintptr_t));

    if (!region) {
        printf("Failed to allocate region: (%lu bytes)\n", sizeof(Region) + size * sizeof(uintptr_t));
        return NULL;
    }
    region->data_count = 0;
    region->capacity = size;
    region->next = NULL;

    return region;
};

void* region_allocate(Region* reg, uint32_t size_bytes)
{

    size_t size = ALIGN_SIZE(size_bytes);

    if (reg->data_count + size > reg->capacity) {
        printf("Tried to region_allocate size (%" PRIu64 ") greater than capacity (%" PRIu32 ")\n", reg->data_count + size, reg->capacity);
        return NULL;
    }

    void* res = &reg->data[reg->data_count];
    reg->data_count += size;

    return res;
}

inline void region_reset(Region* reg)
{
    reg->data_count = 0;
}
inline void region_free(Region* reg)
{
    free(reg);
}

void print_region(Region* reg)
{
    if (!reg) {
        printf("Region is null, could not print.");
        return;
    }
    printf("Used: %" PRIu64 " bytes\n", reg->data_count * sizeof(uintptr_t));
    printf("Capacity: %" PRIu64 " bytes\n", reg->capacity * sizeof(uintptr_t));
}

Arena* create_arena(uint32_t size_bytes)
{
    Arena* arena = (Arena*)malloc(sizeof(Arena));

    arena->start = create_region(size_bytes);
    arena->end = arena->start;

    return arena;
};

void* arena_allocate(Arena* arena, uint32_t size_bytes)
{
    Region* curr = arena->end;

    size_t size = ALIGN_SIZE(size_bytes);

    while (curr->capacity - curr->data_count < size) {
        if (curr->next == NULL) {
            uint32_t new_size = curr->capacity;
            if (size > curr->capacity) {
                new_size = size;
            }
            curr->next = create_region(new_size * sizeof(uintptr_t));
            if (!curr->next) {
                printf("Failed to allocate new region for arena\n");
                return NULL;
            }
        }
        curr = curr->next;
    }
    arena->end = curr;
    return region_allocate(arena->end, size_bytes);
}

void arena_reset(Arena* arena)
{
    Region* curr = arena->start;
    while (curr) {
        region_reset(curr);
        curr = curr->next;
    }
    arena->end = arena->start;
}

void arena_free(Arena* arena)
{
    Region* curr = arena->start;
    while (curr) {
        Region* tmp = curr->next;
        region_free(curr);
        curr = tmp;
    }

    free(arena);
}

void print_arena(Arena* arena)
{
    if (!arena) {
        printf("Arena is NULL\n");
        return;
    }
    uint32_t total_size = 0;
    uint32_t total_used = 0;
    int num_regions = 0;

    Region* curr = arena->start;
    while (curr != NULL) {
        total_size += curr->capacity;
        total_used += curr->data_count;
        num_regions += 1;
        curr = curr->next;
    }

    printf("Total Used: %" PRIu64 " bytes\n", total_used * sizeof(uintptr_t));
    printf("Total Capacity: %" PRIu64 " bytes\n", total_size * sizeof(uintptr_t));
    printf("Num Regions: %i\n", num_regions);
}

#endif // ARENA_IMPLEMENTATION
