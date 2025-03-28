
# STAM | Still Thinking About Memory?
Header Only Arena Allocator in C for worry free, fast, and efficient memory management with little overhead


## Building Examples
```bash
./build.sh
./test_cpp 
./test_c
```


## Example Usage

You must have one file in your project that defines `ARENA_IMPLEMENTATION` before including the library. Other than that, you can just include it like normal.

```C
#define ARENA_IMPLEMENTATION
#include "Arena.h"

int main(){
    typedef struct Point {
        int a;
        int b;
        int c;
    } Point;

    Arena* arr = create_arena(1 MB);
    struct Point* p = (Point*)arena_allocate(arr, sizeof(Point));
    print_arena(arr);

    arena_reset(arr);
    print_arena(arr);
    struct Point* p1 = (Point*)arena_allocate(arr, sizeof(Point));
    struct Point* p2 = (Point*)arena_allocate(arr, sizeof(Point));
    print_arena(arr);

    arena_free(arr);
    
}

```
