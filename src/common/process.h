#pragma once

#include <csignal>
#include <expected>
#include <span>
#include <system_error>

#include "ipc/pipe.h"

class Process {
  public:
    explicit Process(pid_t process_id);

    [[nodiscard]]
    static auto Create(std::initializer_list<const char*> args)
        -> std::expected<Process, std::system_error>;
    [[nodiscard]]
    static auto Create(std::span<const char*> args)
        -> std::expected<Process, std::system_error>;

    [[nodiscard]]
    static auto CreateWithPipe(std::initializer_list<const char*> args,
                               int pipe_fd = STDOUT_FILENO)
        -> std::expected<std::pair<PipeReader, Process>, std::system_error>;
    [[nodiscard]]
    static auto CreateWithPipe(std::span<const char*> args,
                               int pipe_fd = STDOUT_FILENO)
        -> std::expected<std::pair<PipeReader, Process>, std::system_error>;

    [[nodiscard]]
    static auto CreateReady(std::initializer_list<const char*> args)
        -> std::expected<Process, std::system_error>;
    [[nodiscard]]
    static auto CreateReady(std::span<const char*> args)
        -> std::expected<Process, std::system_error>;

    [[nodiscard]] auto TermWait() const
        -> std::expected<int, std::system_error>;
    [[nodiscard]] auto Signal(int signal) const
        -> std::expected<void, std::system_error>;
    [[nodiscard]] auto Wait() const -> std::expected<int, std::system_error>;

    static auto WaitReady(PipeReader& pipe)
        -> std::expected<void, std::system_error>;

  private:
    static void Exec(std::span<const char*> args, int fd_to_keep = 0);

    pid_t process_id_{};
};

class CurrentProcess : public Process {
  public:
    CurrentProcess(const CurrentProcess&) = delete;
    auto operator=(const CurrentProcess&) -> CurrentProcess& = delete;
    CurrentProcess(CurrentProcess&&) = delete;
    auto operator=(CurrentProcess&&) -> CurrentProcess& = delete;
    ~CurrentProcess() = default;

    static auto Get() noexcept -> CurrentProcess&;

    static void AddHandler(int signal, void (*handler)(int));
    static auto SignalReady() -> std::expected<void, std::runtime_error>;
    static auto TerminateReceived() -> bool;

  private:
    static volatile sig_atomic_t terminate_sig_received_;
    CurrentProcess();
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern CurrentProcess& g_curr_process;
