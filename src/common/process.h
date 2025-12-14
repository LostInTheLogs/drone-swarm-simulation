#pragma once

#include <expected>
#include <functional>
#include <span>
#include <stdexcept>
#include <system_error>

class Process {
  public:
    explicit Process(pid_t process_id);

    [[nodiscard]]
    static auto Create(std::span<const char *> args)
        -> std::expected<Process, std::system_error>;

    [[nodiscard]] auto Signal(int signal) const
        -> std::expected<void, std::system_error>;
    [[nodiscard]] auto Wait() const -> std::expected<int, std::system_error>;

  private:
    pid_t process_id_{};
};
