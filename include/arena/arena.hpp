#pragma once
#include <cstddef>
#include <cstdlib>
#include <cstring>
#include <cassert>
#include <new>
#include <type_traits>
#include <utility>

namespace arena {

// Bump allocator: fast O(1) alloc, O(1) free-all (no individual free).
// Thread-unsafe. Single-header, no dependencies.
class Arena {
public:
    explicit Arena(std::size_t capacity)
        : _buf(static_cast<char*>(std::malloc(capacity)))
        , _capacity(capacity)
        , _used(0)
        , _owns(true)
    {
        if (!_buf) throw std::bad_alloc{};
    }

    // Use externally-owned memory. Arena will not free it.
    Arena(void* buf, std::size_t capacity) noexcept
        : _buf(static_cast<char*>(buf))
        , _capacity(capacity)
        , _used(0)
        , _owns(false)
    {}

    Arena(Arena&& o) noexcept
        : _buf(o._buf), _capacity(o._capacity), _used(o._used), _owns(o._owns)
    { o._buf = nullptr; o._owns = false; }

    Arena& operator=(Arena&&) = delete;
    Arena(const Arena&)       = delete;
    Arena& operator=(const Arena&) = delete;

    ~Arena() { if (_owns && _buf) std::free(_buf); }

    // Allocate `size` bytes aligned to `align`. Returns nullptr on OOM.
    void* alloc(std::size_t size, std::size_t align = alignof(std::max_align_t)) noexcept
    {
        assert((align & (align - 1)) == 0 && "align must be power of two");
        const std::size_t start = (_used + align - 1) & ~(align - 1);
        if (start + size > _capacity) return nullptr;
        _used = start + size;
        return _buf + start;
    }

    // Allocate and value-initialize T.
    template <class T, class... Args>
    T* make(Args&&... args)
    {
        void* p = alloc(sizeof(T), alignof(T));
        if (!p) throw std::bad_alloc{};
        return ::new (p) T(std::forward<Args>(args)...);
    }

    // Allocate array of n default-constructed T's.
    template <class T>
    T* make_array(std::size_t n)
    {
        void* p = alloc(sizeof(T) * n, alignof(T));
        if (!p) throw std::bad_alloc{};
        T* arr = static_cast<T*>(p);
        if constexpr (!std::is_trivially_constructible_v<T>)
            for (std::size_t i = 0; i < n; ++i) ::new (arr + i) T{};
        return arr;
    }

    // Copy `size` bytes into the arena. Returns nullptr on OOM.
    void* copy(const void* src, std::size_t size) noexcept
    {
        void* dst = alloc(size);
        if (dst) std::memcpy(dst, src, size);
        return dst;
    }

    // Copy a null-terminated string into the arena (including '\0'). Returns nullptr on OOM.
    const char* strdup(const char* s) noexcept
    {
        const std::size_t n = std::strlen(s) + 1;
        return static_cast<const char*>(copy(s, n));
    }

    // Save / restore a position checkpoint.
    struct Checkpoint { std::size_t used; };
    Checkpoint save() const noexcept         { return {_used}; }
    void       restore(Checkpoint c) noexcept { _used = c.used; }

    // Reset to empty, ready for reuse.
    void reset() noexcept { _used = 0; }

    std::size_t used()      const noexcept { return _used; }
    std::size_t capacity()  const noexcept { return _capacity; }
    std::size_t remaining() const noexcept { return _capacity - _used; }

private:
    char*       _buf;
    std::size_t _capacity;
    std::size_t _used;
    bool        _owns;
};


// Fixed-size object pool — O(1) alloc and free, single type.
template <class T, std::size_t N>
class Pool {
    static_assert(N > 0);

    union Slot {
        alignas(T) char obj[sizeof(T)];
        Slot*           next;
    };

public:
    Pool() noexcept
    {
        for (std::size_t i = 0; i + 1 < N; ++i)
            _slots[i].next = &_slots[i + 1];
        _slots[N - 1].next = nullptr;
        _free = &_slots[0];
    }

    ~Pool()
    {
        // objects must be explicitly freed before pool is destroyed
    }

    template <class... Args>
    T* make(Args&&... args)
    {
        if (!_free) throw std::bad_alloc{};
        Slot* s = _free;
        _free = s->next;
        ++_used;
        return ::new (s->obj) T(std::forward<Args>(args)...);
    }

    void release(T* p) noexcept
    {
        p->~T();
        auto* s  = reinterpret_cast<Slot*>(p);
        s->next  = _free;
        _free    = s;
        --_used;
    }

    std::size_t used()      const noexcept { return _used; }
    std::size_t capacity()  const noexcept { return N; }
    std::size_t remaining() const noexcept { return N - _used; }

private:
    Slot        _slots[N];
    Slot*       _free;
    std::size_t _used = 0;
};

} // namespace arena
