#pragma once
#include <compare>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)

// max heap queue
template <typename T>
class Queue {
  public:
    Queue() = default;
    Queue(Queue &&) = delete;
    Queue(const Queue &) = delete;
    auto operator=(Queue &&) -> Queue & = default;
    auto operator=(const Queue &) -> Queue & = default;
    ~Queue() = default;

    void Push(const T &data, int priority) {
        Item item{.priority = priority, .seq = seq_, .data = data};
        seq_++;

        heap_[size_] = item;
        size_++;
        ShiftUp(size_ - 1);
    }

    auto Peek() -> std::optional<std::reference_wrapper<T>> {
        if (size_ > 0) {
            return heap_[0].data;
        }
        return {};
    }

    auto Pop() -> std::optional<std::reference_wrapper<T>> {
        auto ret = Peek();
        if (!ret) {
            return {};
        }

        std::swap(heap_[0], heap_[size_ - 1]);
        size_--;
        ShiftDown(0);
        return ret;
    }

    void Remove(const T &item) {
        size_t found = 0;
        for (found = 0; found < size_; found++) {
            if (heap_[found].data == item) {
                break;
            }
        }

        if (found == size_) {
            return;
        }

        std::swap(heap_[found], heap_[size_ - 1]);
        size_--;
        ShiftDown(found);
        ShiftUp(found);
    }

    static auto CalcExtraSize(size_t n) -> size_t {
        return (sizeof(Item) * (n - 1));
    }

  private:
    struct Item {
        int priority;
        uint64_t seq;
        T data;
        auto operator<=>(const Item &other) const noexcept
            -> std::strong_ordering {
            if (priority != other.priority) {
                return priority <=> other.priority;
            }

            return static_cast<int64_t>(other.seq - seq) <=> 0;
        }
    };

    static constexpr auto Parent(size_t node) -> size_t {
        return (node - 1) / 2;
    }
    static constexpr auto Left(size_t node) -> size_t {
        return (2 * node) + 1;
    }
    static constexpr auto Right(size_t node) -> size_t {
        return (2 * node) + 2;
    }

    void ShiftUp(size_t node_i) {
        while (true) {
            if (node_i <= 0) {
                break;
            }

            const auto parent_i = Parent(node_i);
            const auto parent = heap_[parent_i];
            const auto node = heap_[node_i];
            if (parent >= node) {
                return;
            }

            std::swap(heap_[node_i], heap_[parent_i]);
            node_i = parent_i;
        }
    }

    void ShiftDown(size_t node) {
        while (true) {
            auto max = node;

            const auto left = Left(node);
            if (left < size_ && heap_[left] > heap_[max]) {
                max = left;
            }

            const auto right = Right(node);
            if (right < size_ && heap_[right] > heap_[max]) {
                max = right;
            }

            if (max == node) {
                return;
            }

            std::swap(heap_[node], heap_[max]);
            node = max;
        }
    }

    uint64_t seq_{};
    size_t size_{};
    Item heap_[1];  // NOLINT(*-c-arrays)
};
// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
