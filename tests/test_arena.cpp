#define ARENA_IMPLEMENTATION
#define ARENA_CPP
#include "../Arena.h"

#include <iostream>

struct TestStruct {
    int x;
    float y;
    TestStruct(int a, float b)
        : x(a)
        , y(b)
    {
    }
};

int main()
{
    std::cout << "Testing C++ Arena Implementation\n";

    ArenaCPP arena(1 KB);

    int* a = arena.allocate<int>();

    if (a) {
        *a = 100;
        std::cout << "Allocated int: " << *a << std::endl;
    } else {
        std::cout << "Failed to allocate int" << std::endl;
    }

    TestStruct* obj = arena.allocate<TestStruct>(5, 2.5f);
    if (obj) {
        std::cout << "Allocated TestStruct: (" << obj->x << ", " << obj->y << ")" << std::endl;
    } else {
        std::cout << "Failed to allocate TestStruct" << std::endl;
    }

    arena.print();

    arena.reset();
    std::cout << "Arena reset.\n";
    arena.print();


    return 0;
}
