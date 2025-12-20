#include "process.h"

#include <fcntl.h>
#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <system_error>
#include <utility>
#include <vector>

#include "ipc/pipe.h"

// NOLINTNEXTLINE(cppcoreguidelines-avoid-non-const-global-variables)
CurrentProcess& g_curr_process = CurrentProcess::Get();

Process::Process(pid_t process_id) : process_id_(process_id) {}

auto Process::Create(std::initializer_list<const char*> args)
    -> std::expected<Process, std::system_error> {
    auto vec = std::vector(args);
    return Create(vec);
}
auto Process::Create(std::span<const char*> args)
    -> std::expected<Process, std::system_error> {
    auto process_id = fork();

    if (process_id == 0) {
        Exec(args);
    } else if (process_id == -1) {
        return std::unexpected(
            std::system_error(errno, std::generic_category()));
    }

    return Process(process_id);
}

auto Process::CreateWithPipe(std::initializer_list<const char*> args,
                             int pipe_fd)
    -> std::expected<std::pair<PipeReader, Process>, std::system_error> {
    auto vec = std::vector(args);
    return CreateWithPipe(vec, pipe_fd);
}
auto Process::CreateWithPipe(std::span<const char*> args, int pipe_fd)
    -> std::expected<std::pair<PipeReader, Process>, std::system_error> {
    std::array<int, 2> pipe_ends{};
    if (pipe(pipe_ends.data()) == -1) {
        return std::unexpected(
            std::system_error(errno, std::generic_category()));
    }

    auto process_id = fork();

    if (process_id == 0) {
        close(pipe_ends[0]);
        dup2(pipe_ends[1], pipe_fd);
        Exec(args, pipe_fd);
    } else if (process_id == -1) {
        close(pipe_ends[0]);
        close(pipe_ends[1]);
        return std::unexpected(
            std::system_error(errno, std::generic_category()));
    }

    close(pipe_ends[1]);
    return std::make_pair(PipeReader(pipe_ends[0]), Process(process_id));
}

auto Process::CreateReady(std::initializer_list<const char*> args)
    -> std::expected<Process, std::system_error> {
    auto vec = std::vector(args);
    return CreateReady(vec);
}
auto Process::CreateReady(std::span<const char*> args)
    -> std::expected<Process, std::system_error> {
    auto pipe_and_process = Process::CreateWithPipe(args, 3);
    if (!pipe_and_process) {
        return std::unexpected(pipe_and_process.error());
    }
    auto& [pipe, logger_process] = pipe_and_process.value();

    if (auto success = Process::WaitReady(pipe); !success) {
        return std::unexpected(success.error());
    }
    return logger_process;
}

void Process::Exec(std::span<const char*> args, int fd_to_keep) {
    // close all but first 3 file descriptors
    auto max_fd = sysconf(_SC_OPEN_MAX);
    if (max_fd == -1) {
        max_fd = 1024;  // fallback NOLINT(readability-magic-numbers)
    }
    for (int fd = 3; fd < max_fd; ++fd) {
        if (fd == fd_to_keep) {
            continue;
        }
        close(fd);
    }

    // clear signal masks
    sigset_t set;
    sigemptyset(&set);
    if (pthread_sigmask(SIG_SETMASK, &set, nullptr) != 0) {
        perror("pthread_sigmask failed");
        _Exit(EXIT_FAILURE);
    }

    std::vector c_args(args.begin(), args.end());
    c_args.emplace_back(nullptr);

    // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
    auto ret = execvp(c_args[0], const_cast<char* const*>(c_args.data()));
    if (ret == -1) {
        perror("execvp failed");
        _Exit(EXIT_FAILURE);
    }
}

auto Process::TermWait() const -> std::expected<int, std::system_error> {
    if (auto success = Signal(SIGTERM); !success) {
        return std::unexpected(success.error());
    }

    return Wait();
}

auto Process::Signal(int signal) const
    -> std::expected<void, std::system_error> {
    if (kill(process_id_, signal) == -1) {
        return std::unexpected(
            std::system_error(errno, std::generic_category()));
    }

    return {};
}

auto Process::Wait() const -> std::expected<int, std::system_error> {
    int status{};

    waitpid(process_id_, &status, 0);

    return status;
}

auto Process::WaitReady(PipeReader& pipe)
    -> std::expected<void, std::system_error> {
    auto read = pipe.Read<char>();
    if (!read) {
        return std::unexpected(read.error());
    }
    return {};
}

CurrentProcess::CurrentProcess() : Process(getpid()) {}

auto CurrentProcess::Get() noexcept -> CurrentProcess& {
    static auto instance = CurrentProcess();
    static auto init = []() {
        auto handler = [](int) { CurrentProcess::terminate_sig_received_ = 1; };
        CurrentProcess::AddHandler(SIGINT, handler);
        CurrentProcess::AddHandler(SIGTERM, handler);
        return true;
    }();

    (void)init;
    return instance;
}

void CurrentProcess::AddHandler(int signal, void (*handler)(int)) {
    struct sigaction action{};
    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    sigaction(signal, &action, nullptr);
}

auto CurrentProcess::SignalReady() -> std::expected<void, std::runtime_error> {
    PipeWriter writer(3, false);
    auto written = writer.Write('\0');
    if (!written) {
        return std::unexpected(written.error());
    }
    return {};
}

auto CurrentProcess::TerminateReceived() -> bool {
    return terminate_sig_received_ == 1;
}

volatile sig_atomic_t CurrentProcess::terminate_sig_received_ = 0;
