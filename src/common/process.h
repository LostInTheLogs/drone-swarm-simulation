#pragma once

#include <csignal>
#include <expected>
#include <span>

#include "ipc/pipe.h"

class Process {
  public:
    Process(const Process&) = delete;
    Process(Process&&) noexcept;
    auto operator=(const Process&) -> Process& = delete;
    auto operator=(Process&&) -> Process& = delete;
    explicit Process(pid_t process_id);
    ~Process();

    [[nodiscard]]
    static auto Create(std::initializer_list<const char*> args) -> Process;
    [[nodiscard]]
    static auto Create(std::span<const char*> args) -> Process;

    [[nodiscard]]
    static auto CreateWithPipe(std::initializer_list<const char*> args,
                               int pipe_fd = STDOUT_FILENO)
        -> std::pair<PipeReader, Process>;
    [[nodiscard]]
    static auto CreateWithPipe(std::span<const char*> args,
                               int pipe_fd = STDOUT_FILENO)
        -> std::pair<PipeReader, Process>;

    [[nodiscard]]
    static auto CreateReady(std::initializer_list<const char*> args) -> Process;
    [[nodiscard]]
    static auto CreateReady(std::span<const char*> args) -> Process;

    auto TermWait() const -> int;  // NOLINT(*nodiscard*)
    auto Signal(int signal) const -> void;
    auto Wait() const -> int;  // NOLINT(*nodiscard*)

    static auto WaitReady(PipeReader& pipe) -> void;

    [[nodiscard]] auto GetPid() const -> pid_t;

  private:
    explicit Process(pid_t process_id, bool joinable);

    static void Exec(std::span<const char*> args, int fd_to_keep = 0);

    pid_t process_id_{};

    bool owner_ = false;
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
    static void SignalReady();
    static auto TerminateReceived() -> bool;

  private:
    static volatile sig_atomic_t terminate_sig_received_;
    // static ThreadMutex terminate_sig_received_mut_;
    CurrentProcess();
};

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
extern CurrentProcess& g_curr_process;
