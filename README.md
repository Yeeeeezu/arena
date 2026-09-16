# arena

header-only arena allocator and fixed-size object pool for C++20. single file, no dependencies, no cmake required.

---

## what it gives you

- **`arena::Arena`** — bump allocator. allocates in O(1), frees everything at once with `reset()`. ideal for per-frame, per-request, or scratch memory.
- **`arena::Pool<T, N>`** — fixed-size typed object pool. O(1) alloc and free of individual objects. no heap fragmentation.

---

## arena usage

```cpp
#include <arena/arena.hpp>

arena::Arena scratch(1024 * 1024); // 1 MB

// raw allocation
int* arr = static_cast<int*>(scratch.alloc(sizeof(int) * 100, alignof(int)));

// typed construction (like new)
struct Vertex { float x, y, z; };
Vertex* v = scratch.make<Vertex>(1.0f, 2.0f, 3.0f);

// array construction
float* buf = scratch.make_array<float>(256);

// string copy
const char* name = scratch.strdup("hello");

// checkpoints — reset to a saved position
auto cp = scratch.save();
scratch.alloc(512);
scratch.restore(cp); // everything after cp is gone

// reset — reuse the whole arena
scratch.reset();
```

## pool usage

```cpp
#include <arena/arena.hpp>

struct Node { int val; Node* next; };

arena::Pool<Node, 64> pool; // 64 slots, on the stack

Node* a = pool.make(1, nullptr);
Node* b = pool.make(2, a);

pool.release(a); // O(1) free, slot goes back to freelist
pool.release(b);
```

## install

single-header. just copy `include/arena/arena.hpp` into your project, or add `include/` to your include path:

```sh
g++ -std=c++20 -Ipath/to/arena/include your_file.cpp
```

## structure

```
include/
  arena/
    arena.hpp   Arena + Pool, ~130 lines
tests/
  test_arena.cpp  31 unit tests
```

## testing

compiled and ran all 31 tests with g++ -std=c++20. pass:

- basic alloc, OOM, reset, checkpoint/restore
- alignment guarantees (8-byte, 16-byte)
- make<T> and make_array<T>
- strdup
- external buffer (arena over stack / pre-allocated memory)
- pool alloc, release, reuse, OOM

```
31 passed, 0 failed
```

## notes

- `Arena` is not thread-safe. one arena per thread, or synchronize externally.
- `Arena::make<T>` calls constructors. `Arena::make_array<T>` skips construction for trivially constructible types.
- `Pool<T, N>` does NOT call destructors on `~Pool`. call `release()` on all live objects first.
- alignment argument to `alloc()` must be a power of two. asserts in debug builds.

## license

MIT
