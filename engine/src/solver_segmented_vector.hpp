#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace poecraft {
namespace solver {
namespace solve_detail {

/* Append-oriented indexed storage whose allocations grow by fixed segments.
 * The exact evaluator uses stable integer ids rather than pointers, so it
 * does not need contiguous storage. Fixed segments avoid std::vector's late
 * capacity doubling while preserving checked O(1) random access. */
template <typename T, std::size_t SegmentElements = 16384>
class SegmentedVector {
    static_assert(SegmentElements != 0);
    static_assert(
        (SegmentElements & (SegmentElements - 1)) == 0,
        "segment size must be a power of two");

public:
    SegmentedVector() = default;
    SegmentedVector(SegmentedVector&& other) noexcept
        : segments_(std::move(other.segments_)),
          size_(std::exchange(other.size_, 0)) {}
    SegmentedVector& operator=(SegmentedVector&& other) noexcept {
        if (this == &other) return *this;
        segments_ = std::move(other.segments_);
        size_ = std::exchange(other.size_, 0);
        return *this;
    }
    SegmentedVector(const SegmentedVector&) = delete;
    SegmentedVector& operator=(const SegmentedVector&) = delete;

    bool empty() const noexcept { return size_ == 0; }
    std::size_t size() const noexcept { return size_; }
    std::size_t capacity() const noexcept {
        return segments_.size() * SegmentElements;
    }

    T& operator[](const std::size_t index) noexcept {
        return segments_[index / SegmentElements][index % SegmentElements];
    }
    const T& operator[](const std::size_t index) const noexcept {
        return segments_[index / SegmentElements][index % SegmentElements];
    }

    T& at(const std::size_t index) {
        if (index >= size_) {
            throw std::out_of_range("segmented vector index out of range");
        }
        return (*this)[index];
    }
    const T& at(const std::size_t index) const {
        if (index >= size_) {
            throw std::out_of_range("segmented vector index out of range");
        }
        return (*this)[index];
    }

    void push_back(const T& value) {
        ensure_tail_segment();
        (*this)[size_] = value;
        ++size_;
    }
    void push_back(T&& value) {
        ensure_tail_segment();
        (*this)[size_] = std::move(value);
        ++size_;
    }

    void assign(const std::size_t count, const T& value) {
        release();
        const std::size_t segment_count = segments_for(count);
        segments_.reserve(segment_count);
        for (std::size_t segment = 0; segment < segment_count; ++segment) {
            segments_.push_back(std::make_unique<T[]>(SegmentElements));
        }
        for (std::size_t index = 0; index < count; ++index) {
            (*this)[index] = value;
        }
        size_ = count;
    }

    void clear() noexcept {
        segments_.clear();
        size_ = 0;
    }
    void release() noexcept {
        decltype(segments_){}.swap(segments_);
        size_ = 0;
    }

    std::uint64_t owned_bytes() const noexcept {
        return static_cast<std::uint64_t>(segments_.capacity()) *
                   sizeof(typename decltype(segments_)::value_type) +
               static_cast<std::uint64_t>(segments_.size()) *
                   SegmentElements * sizeof(T);
    }

    std::uint64_t additional_owned_bytes_for_push() const noexcept {
        if (size_ != capacity()) return 0;
        const std::size_t old_slots = segments_.capacity();
        const std::size_t projected_slots =
            old_slots == 0 ? 1 : old_slots * 2;
        return static_cast<std::uint64_t>(SegmentElements) * sizeof(T) +
               static_cast<std::uint64_t>(projected_slots - old_slots) *
                   sizeof(typename decltype(segments_)::value_type);
    }

    static std::uint64_t projected_owned_bytes(
            const std::size_t count) noexcept {
        const std::size_t segment_count = segments_for(count);
        return static_cast<std::uint64_t>(segment_count) *
               (static_cast<std::uint64_t>(SegmentElements) * sizeof(T) +
                sizeof(typename decltype(segments_)::value_type));
    }

private:
    static constexpr std::size_t segments_for(
            const std::size_t count) noexcept {
        return count == 0
            ? 0
            : 1 + (count - 1) / SegmentElements;
    }

    void ensure_tail_segment() {
        if (size_ != capacity()) return;
        if (segments_.size() == segments_.capacity()) {
            segments_.reserve(
                segments_.capacity() == 0
                    ? 1
                    : segments_.capacity() * 2);
        }
        segments_.push_back(std::make_unique<T[]>(SegmentElements));
    }

    std::vector<std::unique_ptr<T[]>> segments_;
    std::size_t size_ = 0;
};

} // namespace solve_detail
} // namespace solver
} // namespace poecraft
