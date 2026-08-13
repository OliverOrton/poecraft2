#pragma once

#include <coroutine>
#include <cstddef>
#include <cstdint>
#include <exception>
#include <limits>
#include <new>
#include <optional>
#include <stdexcept>
#include <utility>

namespace poecraft {
namespace solver {
namespace solve_detail {

struct CooperativeCheckpoint {
    std::size_t retained_nested_bytes = 0;

    constexpr CooperativeCheckpoint() = default;

    constexpr explicit CooperativeCheckpoint(
        const std::uint64_t retained_bytes) noexcept
        : retained_nested_bytes(
              retained_bytes >
                      static_cast<std::uint64_t>(
                          std::numeric_limits<std::size_t>::max())
                  ? std::numeric_limits<std::size_t>::max()
                  : static_cast<std::size_t>(retained_bytes)) {}
};

/*
 * A single-threaded, explicitly resumed continuation.  The engine/WASM
 * worker owns the task and resumes it only from SolveWork::step; no detached
 * work survives cancellation.  The allocation header makes the coroutine
 * frame an honest selected-owned byte in the solver ledger.
 */
template <typename T>
class CooperativeTask {
  public:
    struct promise_type;
    using Handle = std::coroutine_handle<promise_type>;

    struct promise_type {
        struct alignas(std::max_align_t) AllocationHeader {
            std::size_t size = 0;
        };
        std::optional<T> value;
        std::exception_ptr exception;
        std::size_t retained_nested_bytes = 0;

        static void* operator new(const std::size_t size) {
            auto* storage = static_cast<std::byte*>(
                ::operator new(size + sizeof(AllocationHeader)));
            reinterpret_cast<AllocationHeader*>(storage)->size = size;
            return storage + sizeof(AllocationHeader);
        }

        static void operator delete(void* pointer) noexcept {
            if (pointer == nullptr) return;
            auto* storage = static_cast<std::byte*>(pointer) -
                            sizeof(AllocationHeader);
            ::operator delete(storage);
        }

        static void operator delete(
            void* pointer, const std::size_t) noexcept {
            operator delete(pointer);
        }

        CooperativeTask get_return_object() noexcept {
            return CooperativeTask{
                Handle::from_promise(*this)};
        }

        std::suspend_always initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }

        struct CheckpointAwaiter {
            promise_type* promise = nullptr;
            std::size_t nested_bytes = 0;

            bool await_ready() const noexcept { return false; }
            void await_suspend(const Handle) const noexcept {
                promise->retained_nested_bytes = nested_bytes;
            }
            void await_resume() const noexcept {}
        };

        CheckpointAwaiter await_transform(
            const CooperativeCheckpoint checkpoint) noexcept {
            return {this, checkpoint.retained_nested_bytes};
        }

        template <typename U>
        void return_value(U&& returned) {
            value.emplace(std::forward<U>(returned));
        }

        void unhandled_exception() noexcept {
            exception = std::current_exception();
        }
    };

    CooperativeTask() = default;

    explicit CooperativeTask(const Handle handle) noexcept
        : handle_(handle) {}

    ~CooperativeTask() { reset(); }

    CooperativeTask(const CooperativeTask&) = delete;
    CooperativeTask& operator=(const CooperativeTask&) = delete;

    CooperativeTask(CooperativeTask&& other) noexcept
        : handle_(std::exchange(other.handle_, {})) {}

    CooperativeTask& operator=(CooperativeTask&& other) noexcept {
        if (this == &other) return *this;
        reset();
        handle_ = std::exchange(other.handle_, {});
        return *this;
    }

    bool done() const noexcept {
        return handle_ == nullptr || handle_.done();
    }

    bool resume() {
        if (handle_ == nullptr) {
            throw std::logic_error("resumed an empty cooperative task");
        }
        if (!handle_.done()) handle_.resume();
        if (handle_.done() && handle_.promise().exception != nullptr) {
            std::rethrow_exception(handle_.promise().exception);
        }
        return handle_.done();
    }

    T take_result() {
        if (handle_ == nullptr || !handle_.done()) {
            throw std::logic_error(
                "cooperative task result requested before completion");
        }
        if (handle_.promise().exception != nullptr) {
            std::rethrow_exception(handle_.promise().exception);
        }
        if (!handle_.promise().value.has_value()) {
            throw std::logic_error(
                "completed cooperative task omitted its result");
        }
        return std::move(*handle_.promise().value);
    }

    std::size_t frame_bytes() const noexcept {
        if (handle_ == nullptr) return 0;
        const auto* storage =
            static_cast<const std::byte*>(handle_.address()) -
            sizeof(typename promise_type::AllocationHeader);
        return reinterpret_cast<
            const typename promise_type::AllocationHeader*>(storage)->size;
    }

    std::size_t retained_bytes() const noexcept {
        return frame_bytes() +
               (handle_ == nullptr
                    ? 0
                    : handle_.promise().retained_nested_bytes);
    }

    void reset() noexcept {
        if (handle_ == nullptr) return;
        handle_.destroy();
        handle_ = {};
    }

  private:
    Handle handle_{};
};

} // namespace solve_detail
} // namespace solver
} // namespace poecraft
