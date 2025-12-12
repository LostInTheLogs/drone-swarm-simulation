#pragma once

#include <expected>
#include <functional>
#include <stdexcept>
#include <system_error>

class Thread {
  private:
    using Callable = std::function<void()>;

  public:
    [[nodiscard]] static auto Create(const Callable &function)
        -> std::expected<Thread, std::system_error>;
    [[nodiscard]] auto Join() const -> std::expected<void, std::system_error>;
    [[nodiscard]] auto Cancel() const -> std::expected<void, std::system_error>;

  private:
    pthread_t thread_id_{};
};
