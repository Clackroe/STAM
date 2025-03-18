
#include <algorithm>
#include <cassert>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numeric>
#include <string>
#include <vector>

// Include our arena implementation
#define ARENA_IMPLEMENTATION
#define ARENA_CPP
#include "../Arena.h" // Make sure this points to your arena.h file

// Test struct with non-trivial construction/destruction
struct TestObject {
    int id;
    std::string name;
    std::vector<int> data;

    TestObject()
        : id(0)
        , name("default")
        , data(10, 0)
    {
    }

    TestObject(int id, const std::string& name)
        : id(id)
        , name(name)
        , data(id % 10 + 1, id)
    {
    }

    // Add some data to make the object non-trivial
    void populate()
    {
        for (size_t i = 0; i < data.size(); i++) {
            data[i] = id * 100 + i;
        }
    }

    // Add a method to verify object integrity
    bool verify() const
    {
        if (data.size() != (id % 10 + 1))
            return false;
        for (size_t i = 0; i < data.size(); i++) {
            if (data[i] != id * 100 + i)
                return false;
        }
        return true;
    }
};

// Timer utility class
class Timer {
private:
    using clock_t = std::chrono::high_resolution_clock;
    using second_t = std::chrono::duration<double, std::ratio<1>>;
    std::chrono::time_point<clock_t> start_time;

public:
    Timer()
        : start_time(clock_t::now())
    {
    }

    void reset()
    {
        start_time = clock_t::now();
    }

    double elapsed() const
    {
        return std::chrono::duration_cast<second_t>(clock_t::now() - start_time).count();
    }
};

// Test results structure
struct TestResult {
    std::string test_name;
    double std_alloc_time;
    double arena_alloc_time;
    size_t memory_used;
    size_t operations;
    bool passed;

    // Print nicely formatted results
    void print() const
    {
        std::cout << std::left << std::setw(30) << test_name
                  << " | STD: " << std::setw(10) << std::fixed << std::setprecision(6) << std_alloc_time << " sec"
                  << " | Arena: " << std::setw(10) << std::fixed << std::setprecision(6) << arena_alloc_time << " sec"
                  << " | Diff: " << std::setw(10) << std::fixed << std::setprecision(2)
                  << (std_alloc_time / arena_alloc_time) << "x faster"
                  << " | Memory: " << std::setw(8) << memory_used / 1024 << " KB"
                  << " | " << (passed ? "PASSED" : "FAILED") << std::endl;
    }
};

// ===== TEST CASES =====

// Test 1: Simple allocation and deallocation
TestResult test_simple_allocation(size_t count)
{
    TestResult result;
    result.test_name = "Simple Allocation";
    result.operations = count;
    result.passed = true;

    // Standard allocator test
    {
        Timer timer;
        for (size_t i = 0; i < count; i++) {
            int* p = new int(i);
            delete p;
        }
        result.std_alloc_time = timer.elapsed();
    }

    // Arena allocator test
    {
        Timer timer;
        ArenaCPP arena(64 MB);
        for (size_t i = 0; i < count; i++) {
            int* p = arena.allocate<int>(i);
            // No delete needed
        }
        result.arena_alloc_time = timer.elapsed();

        // Get memory usage
        result.memory_used = arena.arena->start->capacity * sizeof(uintptr_t);
    }

    return result;
}

// Test 2: Vector resizing behavior
TestResult test_vector_resizing(size_t count)
{
    TestResult result;
    result.test_name = "Vector Resizing";
    result.operations = count;
    result.passed = true;

    // Standard vector test
    {
        Timer timer;
        std::vector<int> vec;
        for (size_t i = 0; i < count; i++) {
            vec.push_back(i);
        }
        result.std_alloc_time = timer.elapsed();
    }

    // Arena vector test
    {
        Timer timer;
        ArenaCPP arena(64 MB);
        ArenaVector<int> arena_vec((&arena));
        for (size_t i = 0; i < count; i++) {
            arena_vec.push_back(i);
        }
        result.arena_alloc_time = timer.elapsed();

        // Verify data integrity
        for (size_t i = 0; i < count; i++) {
            if (arena_vec[i] != static_cast<int>(i)) {
                result.passed = false;
                break;
            }
        }

        result.memory_used = arena.arena->start->capacity * sizeof(uintptr_t);
    }

    return result;
}

// Test 3: Complex object allocation
TestResult test_complex_objects(size_t count)
{
    TestResult result;
    result.test_name = "Complex Object Allocation";
    result.operations = count;
    result.passed = true;

    // Standard allocation test
    {
        Timer timer;
        std::vector<TestObject*> objects;
        objects.reserve(count);

        for (size_t i = 0; i < count; i++) {
            TestObject* obj = new TestObject(i, "object_" + std::to_string(i));
            obj->populate();
            objects.push_back(obj);
        }

        // Verify objects
        for (size_t i = 0; i < count; i++) {
            if (!objects[i]->verify()) {
                result.passed = false;
            }
        }

        // Cleanup
        for (auto obj : objects) {
            delete obj;
        }

        result.std_alloc_time = timer.elapsed();
    }

    // Arena allocation test
    {
        Timer timer;
        ArenaCPP arena(64 MB);
        std::vector<TestObject*> objects;
        objects.reserve(count);

        for (size_t i = 0; i < count; i++) {
            TestObject* obj = arena.allocate<TestObject>(i, "object_" + std::to_string(i));
            obj->populate();
            objects.push_back(obj);
        }

        // Verify objects
        for (size_t i = 0; i < count; i++) {
            if (!objects[i]->verify()) {
                result.passed = false;
            }
        }

        // No cleanup needed with arena

        result.arena_alloc_time = timer.elapsed();
        result.memory_used = arena.arena->start->capacity * sizeof(uintptr_t);
    }

    return result;
}

// Test 4: Free list arena test
TestResult test_freelist_arena(size_t count, size_t cycle_size)
{
    TestResult result;
    result.test_name = "Free List Recycling";
    result.operations = count;
    result.passed = true;

    // Standard allocation with explicit free
    {
        Timer timer;
        std::vector<int*> pointers(cycle_size, nullptr);

        for (size_t i = 0; i < count; i++) {
            size_t index = i % cycle_size;
            if (pointers[index] != nullptr) {
                delete pointers[index];
            }
            pointers[index] = new int(i);
        }

        // Cleanup
        for (auto& ptr : pointers) {
            if (ptr)
                delete ptr;
        }

        result.std_alloc_time = timer.elapsed();
    }

    // Arena with free list
    {
        Timer timer;
        ArenaCPP arena(64 MB, true); // Use free list
        std::vector<int*> pointers(cycle_size, nullptr);

        for (size_t i = 0; i < count; i++) {
            size_t index = i % cycle_size;
            if (pointers[index] != nullptr) {
                arena.deallocate(pointers[index], sizeof(int));
            }
            pointers[index] = arena.allocate<int>(i);
        }

        result.arena_alloc_time = timer.elapsed();
        result.memory_used = arena.arena->start->capacity * sizeof(uintptr_t);

        // Verify final values
        for (size_t i = 0; i < cycle_size; i++) {
            int expected = static_cast<int>((count - cycle_size + i) % count);
            if (*pointers[i] != expected) {
                result.passed = false;
                break;
            }
        }
    }

    return result;
}

// Test 5: Map operations (insertion and lookup)
TestResult test_map_operations(size_t count)
{
    TestResult result;
    result.test_name = "Map Operations";
    result.operations = count;
    result.passed = true;

    // Standard map test
    {
        Timer timer;
        std::map<int, std::string> std_map;

        // Insert
        for (size_t i = 0; i < count; i++) {
            std_map[i] = "value_" + std::to_string(i);
        }

        // Lookup
        std::string temp;
        for (size_t i = 0; i < count; i++) {
            temp = std_map[i % count];
        }

        result.std_alloc_time = timer.elapsed();
    }

    // Arena map test
    {
        Timer timer;
        ArenaCPP arena(64 MB);
        ArenaMap<int, std::string> arena_map(&arena);

        // Insert
        for (size_t i = 0; i < count; i++) {
            arena_map[i] = "value_" + std::to_string(i);
        }

        // Lookup
        std::string temp;
        for (size_t i = 0; i < count; i++) {
            temp = arena_map[i % count];
        }

        result.arena_alloc_time = timer.elapsed();
        result.memory_used = arena.arena->start->capacity * sizeof(uintptr_t);

        // Verify map contents
        for (size_t i = 0; i < count; i++) {
            if (arena_map[i] != "value_" + std::to_string(i)) {
                result.passed = false;
                break;
            }
        }
    }

    return result;
}

// Test 6: Arena mark/reset functionality
TestResult test_arena_mark_reset(size_t count, size_t iterations)
{
    TestResult result;
    result.test_name = "Mark and Reset";
    result.operations = count * iterations;
    result.passed = true;

    // Standard approach with vectors
    {
        Timer timer;
        std::vector<int> master_vec;

        for (size_t iter = 0; iter < iterations; iter++) {
            std::vector<int> temp_vec;
            for (size_t i = 0; i < count; i++) {
                temp_vec.push_back(i);
            }

            // Process temp data and update master
            int sum = std::accumulate(temp_vec.begin(), temp_vec.end(), 0);
            master_vec.push_back(sum);
        }

        result.std_alloc_time = timer.elapsed();
    }

    // Arena approach with mark/reset
    {
        Timer timer;
        ArenaCPP arena(64 MB);
        ArenaVector<int> master_vec(&arena);

        for (size_t iter = 0; iter < iterations; iter++) {
            // Set a mark for temporary data
            ArenaMark mark = arena.mark();

            ArenaVector<int> temp_vec(&arena);
            for (size_t i = 0; i < count; i++) {
                temp_vec.push_back(i);
            }

            // Process temp data and update master
            int sum = std::accumulate(temp_vec.begin(), temp_vec.end(), 0);
            master_vec.push_back(sum);

            // Reset to mark, wiping out temp data
            arena.reset(mark);
        }

        result.arena_alloc_time = timer.elapsed();
        result.memory_used = arena.arena->start->capacity * sizeof(uintptr_t);

        // Verify final results - master vector should have sums
        if (master_vec.size() != iterations) {
            result.passed = false;
        } else {
            for (size_t i = 0; i < iterations; i++) {
                int expected_sum = (count * (count - 1)) / 2; // Sum of 0..(count-1)
                if (master_vec[i] != expected_sum) {
                    result.passed = false;
                    break;
                }
            }
        }
    }

    return result;
}
// #define ARENA_IMPLEMENTATION
// #define ARENA_CPP
// #include "../Arena.h"
//
// #include <iostream>
//
// struct TestStruct {
//     int x;
//     float y;
//     TestStruct(int a, float b)
//         : x(a)
//         , y(b)
//     {
//     }
// };
//
// int main()
// {
//     std::cout << "Testing C++ Arena Implementation\n";
//
//     ArenaCPP arena(1 KB);
//
//     int* a = arena.allocate<int>();
//
//     if (a) {
//         *a = 100;
//         std::cout << "Allocated int: " << *a << std::endl;
//     } else {
//         std::cout << "Failed to allocate int" << std::endl;
//     }
//
//     TestStruct* obj = arena.allocate<TestStruct>(5, 2.5f);
//     if (obj) {
//         std::cout << "Allocated TestStruct: (" << obj->x << ", " << obj->y << ")" << std::endl;
//     } else {
//         std::cout << "Failed to allocate TestStruct" << std::endl;
//     }
//
//     arena.print();
//
//     arena.reset();
//     std::cout << "Arena reset.\n";
//     arena.print();
//
//
//     return 0;
// }
