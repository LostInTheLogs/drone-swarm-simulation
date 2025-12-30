#pragma once
#include <cstddef>
#include <cstdio>

// NOLINTBEGIN(cppcoreguidelines-pro-bounds-constant-array-index)

template <typename T>
class Array {
  public:
    Array() = default;
    Array(Array &&) = delete;
    Array(const Array &) = delete;
    auto operator=(Array &&) -> Array & = default;
    auto operator=(const Array &) -> Array & = default;
    ~Array() = default;

    void Insert(const T &data) {
        items_[size_] = data;
        size_++;
    }

    void Remove(T item) {
        size_t found = 0;
        for (found = 0; found < size_; found++) {
            if (items_[found] == item) {
                break;
            }
        }

        if (found == size_) {
            return;
        }

        for (auto i = found; i + 1 < size_; i++) {
            items_[i] = items_[i + 1];
        }

        size_--;
    }

    auto operator[](size_t index) -> T & {
        return items_[index];
    }

    auto Size() -> size_t {
        return size_;
    }

    void Clear() {
        size_ = 0;
    }

    static auto CalcSize(size_t n) -> size_t {
        return sizeof(Array<T>) + (sizeof(T) * (n - 1));
    }

  private:
    size_t size_{};
    T items_[1];  // NOLINT(*-c-arrays)
};
// NOLINTEND(cppcoreguidelines-pro-bounds-constant-array-index)
