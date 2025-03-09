
# STAM | Still Thinking About Memory?
Header Only Arena Allocator in C for worry free, fast, and efficient memory management with little overhead


## Building Example
```bash
./build.sh && main
```


## Example Usage

You must have one file in your project that defines `ARENA_IMPLEMENTATION` before including the library. Other than that, you can just include it like normal.

```C
#define ARENA_IMPLEMENTATION
#include "Arena.h"

int main(){
    struct Point {
        int a;
        int b;
        int c;
    };

    Arena* arr = create_arena(1 MB);
    struct Point* p = (struct Point*)arena_allocate(arr, sizeof(struct Point));
    print_arena(arr);

    arena_reset(arr);
    print_arena(arr);
    struct Point* p1 = (struct Point*)arena_allocate(arr, sizeof(struct Point));
    struct Point* p2 = (struct Point*)arena_allocate(arr, sizeof(struct Point));
    print_arena(arr);

    arena_free(arr);
    
}

```
