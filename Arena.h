#ifndef _ARENA_H
#define _ARENA_H

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#define KB *1024
#define MB *1024 * 1024
#define GB *1024 * 1024 * 1024

#define ALIGN_SIZE(size_bytes) ((size_bytes + sizeof(uintptr_t) - 1) / sizeof(uintptr_t))

typedef struct FreeListNode {
    uint32_t size_bytes;
    void* ptr;
    struct FreeListNode* next;
} FreeListNode;

typedef struct Region {
    uint32_t data_count;
    uint32_t capacity;
    struct Region* next;
    uintptr_t data[];
} Region;

typedef struct Arena {
    Region* start;
    Region* end;
    FreeListNode* free_list;
    bool use_free_list;
} Arena;

typedef struct ArenaMark {
    Region* reg;
    uint32_t count;
} ArenaMark;

Region* create_region(uint32_t size_bytes);
void* region_allocate(Region* reg, uint32_t size_bytes);
void region_reset(Region* reg);
void region_free(Region* reg);
void print_region(Region* reg);

Arena* create_arena(uint32_t size_bytes);
Arena* create_freelist_arena(uint32_t size_bytes);
void* arena_allocate(Arena* arena, uint32_t size_bytes);
void arena_reset(Arena* arena);
void arena_free(Arena* arena);
void print_arena(Arena* arena);
void arena_add_free_list(Arena* arena, void* ptr, size_t size);

ArenaMark arena_scratch(Arena* arena);
void arena_pop_scratch(Arena* arena, ArenaMark m);

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
    arena->free_list = NULL;
    arena->use_free_list = 0;

    return arena;
};
Arena* create_freelist_arena(uint32_t size_bytes)
{
    Arena* arena = create_arena(size_bytes);
    arena->use_free_list = 1;

    return arena;
}

void* arena_allocate(Arena* arena, uint32_t size_bytes)
{
    size_t size = ALIGN_SIZE(size_bytes);

    if (arena->use_free_list && arena->free_list != NULL) {
        FreeListNode* curr_node = arena->free_list;
        FreeListNode* prev = NULL;
        while (curr_node) {
            if (curr_node->size_bytes >= size_bytes) { // Should be fine?
                if (prev) {
                    prev->next = curr_node->next;
                } else {
                    arena->free_list = curr_node->next;
                }
                return curr_node->ptr;
            }
            prev = curr_node;
            curr_node = curr_node->next;
        }
    }

    Region* curr = arena->end;

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

ArenaMark arena_scratch(Arena* arena)
{
    ArenaMark mark;
    if (arena->end == NULL) {
        printf("Tried to make a scrach arena for an uninitialized arena");
        return mark;
    }
    mark.reg = arena->end;
    mark.count = arena->end->data_count;

    return mark;
}
void arena_pop_scratch(Arena* arena, ArenaMark m)
{
    if (m.reg == NULL) {
        arena_reset(arena);
        return;
    }
    m.reg->data_count = m.count;
    Region* curr = m.reg->next;
    while (curr) {
        curr->data_count = 0;
        curr = curr->next;
    }
    arena->end = m.reg;
}

void arena_reset(Arena* arena)
{
    Region* curr = arena->start;
    while (curr) {
        region_reset(curr);
        curr = curr->next;
    }
    arena->end = arena->start;
    arena->free_list = NULL;
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

void arena_add_free_list(Arena* arena, void* ptr, size_t size_bytes)
{
    if (!arena->use_free_list) {
        return;
    }

    if (size_bytes < sizeof(FreeListNode)) { // FreeListNode is stored in the mem block
        printf("ARENA::Error: Attempted to free a block too small for the free list.\n");
        return;
    }

    FreeListNode* node = (FreeListNode*)ptr;
    node->ptr = ptr;
    node->size_bytes = size_bytes;
    node->next = NULL;

    // Attempt to merge adjacent blocks of memory for better fragmentation handling

    FreeListNode* prev = arena->free_list;
    FreeListNode* curr = arena->free_list;
    if (!prev) {
        arena->free_list = node;
        return;
    }

    // Sort by Pointer address...
    while (curr && curr < node) {
        prev = curr;
        curr = curr->next;
    }

    // Now curr is greater than the node's address, lets insert between prev & curr
    prev->next = node;
    node->next = curr;

    // Try to merge blocks
    curr = arena->free_list;
    while (curr && curr->next) {
        if ((uintptr_t)curr->ptr + curr->size_bytes == (uintptr_t)curr->next->ptr) {
            curr->size_bytes += curr->next->size_bytes;
            curr->next = curr->next->next;
        }
        curr = curr->next;
    }
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

#ifdef ARENA_CPP
#include <cstdint>

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
    void print()
    {
        print_arena(arena);
    }

private:
    Arena* arena;
};

#endif // ARENA_CPP

#endif // ARENA_IMPLEMENTATION
#endif //_ARENA_H
