#include <arena/arena.hpp>
#include <cassert>
#include <cstring>
#include <cstdio>
#include <cstdint>
#include <string>

static int passed = 0, failed = 0;

#define EXPECT(expr) do { \
    if (!(expr)) { std::printf("FAIL  line %d: %s\n", __LINE__, #expr); ++failed; } \
    else         { ++passed; } \
} while (0)

// ---- Arena tests ----

static void test_basic_alloc()
{
    arena::Arena a(1024);
    EXPECT(a.used()     == 0);
    EXPECT(a.capacity() == 1024);

    auto* p = static_cast<int*>(a.alloc(sizeof(int), alignof(int)));
    EXPECT(p != nullptr);
    *p = 42;
    EXPECT(*p == 42);
    EXPECT(a.used() >= sizeof(int));
}

static void test_alignment()
{
    arena::Arena a(4096);
    a.alloc(1); // bump by 1 byte to misalign

    auto* p8 = a.alloc(8, 8);
    EXPECT(reinterpret_cast<std::uintptr_t>(p8) % 8 == 0);

    auto* p16 = a.alloc(16, 16);
    EXPECT(reinterpret_cast<std::uintptr_t>(p16) % 16 == 0);
}

static void test_oom()
{
    arena::Arena a(16);
    auto* p = a.alloc(8);
    EXPECT(p != nullptr);
    auto* q = a.alloc(16); // won't fit
    EXPECT(q == nullptr);
}

static void test_reset()
{
    arena::Arena a(256);
    a.alloc(64);
    EXPECT(a.used() == 64);
    a.reset();
    EXPECT(a.used() == 0);
    auto* p = a.alloc(64);
    EXPECT(p != nullptr);
}

static void test_checkpoint()
{
    arena::Arena a(256);
    a.alloc(32);
    auto cp = a.save();
    a.alloc(64);
    EXPECT(a.used() == 96);
    a.restore(cp);
    EXPECT(a.used() == 32);
}

static void test_make()
{
    arena::Arena a(256);
    struct Point { int x, y; };
    auto* pt = a.make<Point>(3, 7);
    EXPECT(pt->x == 3 && pt->y == 7);
}

static void test_make_array()
{
    arena::Arena a(1024);
    auto* arr = a.make_array<int>(10);
    for (int i = 0; i < 10; ++i) arr[i] = i * i;
    EXPECT(arr[9] == 81);
}

static void test_strdup()
{
    arena::Arena a(256);
    const char* s = a.strdup("hello, arena");
    EXPECT(s != nullptr);
    EXPECT(std::strcmp(s, "hello, arena") == 0);
}

static void test_external_buf()
{
    char buf[512];
    arena::Arena a(buf, sizeof(buf));
    auto* p = a.alloc(32);
    EXPECT(p >= static_cast<void*>(buf));
    EXPECT(p <  static_cast<void*>(buf + 512));
}

// ---- Pool tests ----

struct Node { int val; Node* next = nullptr; };

static void test_pool_basic()
{
    arena::Pool<Node, 8> pool;
    EXPECT(pool.capacity()  == 8);
    EXPECT(pool.used()      == 0);
    EXPECT(pool.remaining() == 8);

    auto* n = pool.make(42, nullptr);
    EXPECT(n->val == 42);
    EXPECT(pool.used() == 1);

    pool.release(n);
    EXPECT(pool.used() == 0);
}

static void test_pool_reuse()
{
    arena::Pool<Node, 4> pool;
    Node* ptrs[4];
    for (int i = 0; i < 4; ++i) ptrs[i] = pool.make(i, nullptr);
    EXPECT(pool.remaining() == 0);

    pool.release(ptrs[2]);
    auto* reused = pool.make(99, nullptr);
    EXPECT(reused->val == 99);
    EXPECT(pool.used() == 4);

    for (int i = 0; i < 4; ++i) if (i != 2) pool.release(ptrs[i]);
    pool.release(reused);
    EXPECT(pool.used() == 0);
}

static void test_pool_oom()
{
    arena::Pool<Node, 2> pool;
    auto* a = pool.make(1, nullptr);
    auto* b = pool.make(2, nullptr);
    bool threw = false;
    try   { pool.make(3, nullptr); }
    catch (const std::bad_alloc&) { threw = true; }
    EXPECT(threw);
    pool.release(a); pool.release(b);
}

int main()
{
    test_basic_alloc();
    test_alignment();
    test_oom();
    test_reset();
    test_checkpoint();
    test_make();
    test_make_array();
    test_strdup();
    test_external_buf();
    test_pool_basic();
    test_pool_reuse();
    test_pool_oom();

    std::printf("\n%d passed, %d failed\n", passed, failed);
    return failed != 0 ? 1 : 0;
}
