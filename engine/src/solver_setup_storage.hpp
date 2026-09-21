#pragma once

#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <stdexcept>
#include <unordered_map>
#include <vector>

namespace poecraft::solver::solve_detail {

// Invocation-owned setup containers retain this domain through suspension.
// Only their construction/resumption installs the allocation scope. Nothing
// detached owns this pointer, and the task is destroyed before the domain.
struct SetupStorage {
    std::uint64_t live = 0;
    std::uint64_t reserved = 0;
    void* owner = nullptr;
    void (*admit)(void*, std::uint64_t) = nullptr;
    inline static thread_local SetupStorage* current = nullptr;
    struct Scope {
        SetupStorage* previous;
        explicit Scope(SetupStorage& storage) : previous(current) { current = &storage; }
        ~Scope() { current = previous; }
    };
    struct Reservation {
        SetupStorage& storage;
        std::uint64_t bytes;
        Reservation(SetupStorage& owner, std::uint64_t amount) : storage(owner), bytes(amount) {
            if (storage.admit) storage.admit(storage.owner, bytes);
            storage.reserved += bytes;
        }
        ~Reservation() { storage.reserved -= bytes; }
    };
};

template<class T> struct SetupAllocator {
    using value_type = T;
    SetupStorage* storage = SetupStorage::current;
    SetupAllocator() = default;
    template<class U> SetupAllocator(const SetupAllocator<U>& other) noexcept : storage(other.storage) {}
    T* allocate(std::size_t count) {
        if (count > std::numeric_limits<std::size_t>::max() / sizeof(T)) throw std::bad_array_new_length();
        const auto bytes = count * sizeof(T);
        if (storage && storage->admit) storage->admit(storage->owner, bytes);
        T* result = std::allocator<T>{}.allocate(count);
        if (storage) storage->live += bytes;
        return result;
    }
    void deallocate(T* pointer, std::size_t count) noexcept {
        std::allocator<T>{}.deallocate(pointer, count);
        if (storage) storage->live -= count * sizeof(T);
    }
    template<class U> bool operator==(const SetupAllocator<U>& other) const noexcept { return storage == other.storage; }
};

template<class T> using SetupVector = std::vector<T, SetupAllocator<T>>;
template<class K, class V> using SetupMap = std::map<K, V, std::less<K>, SetupAllocator<std::pair<const K, V>>>;
template<class K, class V> using SetupHashMap = std::unordered_map<K, V, std::hash<K>, std::equal_to<K>, SetupAllocator<std::pair<const K, V>>>;

} // namespace poecraft::solver::solve_detail
