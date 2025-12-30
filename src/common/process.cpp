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

Process::Process(pid_t process_id, bool joinable)
    : process_id_(process_id), owner_(joinable) {}

Process::Process(Process&& other) noexcept
    : process_id_(other.process_id_), owner_(other.owner_) {
    other.owner_ = false;
}
Process::~Process() {
    if (owner_) {
        try {
            Signal(SIGTERM);
        } catch (const std::system_error&) {  // NOLINT
        }
    }
}

auto Process::Create(std::initializer_list<const char*> args) -> Process {
    auto vec = std::vector(args);
    return Create(vec);
}
auto Process::Create(std::span<const char*> args) -> Process {
    auto process_id = fork();

    if (process_id == 0) {
        Exec(args);
    } else if (process_id == -1) {
        throw std::system_error(errno, std::generic_category());
    }

    return Process(process_id, true);
}

auto Process::CreateWithPipe(std::initializer_list<const char*> args,
                             int pipe_fd) -> std::pair<PipeReader, Process> {
    auto vec = std::vector(args);
    return CreateWithPipe(vec, pipe_fd);
}
auto Process::CreateWithPipe(std::span<const char*> args, int pipe_fd)
    -> std::pair<PipeReader, Process> {
    std::array<int, 2> pipe_ends{};
    if (pipe(pipe_ends.data()) == -1) {
        throw std::system_error(errno, std::generic_category());
    }

    auto process_id = fork();

    if (process_id == 0) {
        close(pipe_ends[0]);
        dup2(pipe_ends[1], pipe_fd);
        Exec(args, pipe_fd);
    } else if (process_id == -1) {
        close(pipe_ends[0]);
        close(pipe_ends[1]);
        throw std::system_error(errno, std::generic_category());
    }

    close(pipe_ends[1]);
    return std::make_pair(PipeReader(pipe_ends[0]), Process(process_id, true));
}

auto Process::CreateReady(std::initializer_list<const char*> args) -> Process {
    auto vec = std::vector(args);
    return CreateReady(vec);
}
auto Process::CreateReady(std::span<const char*> args) -> Process {
    auto pipe_and_process = Process::CreateWithPipe(args, 3);
    auto& [pipe, process] = pipe_and_process;

    Process::WaitReady(pipe);
    return std::move(process);
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

auto Process::TermWait() const -> int {
    Signal(SIGTERM);
    return Wait();
}

auto Process::Signal(int signal) const -> void {
    if (kill(process_id_, signal) == -1) {
        throw std::system_error(errno, std::generic_category());
    }
}

auto Process::Wait() const -> int {
    int status{};

    while (true) {
        auto success = waitpid(process_id_, &status, 0);
        const auto interrupted = success == -1 && errno == EINTR;
        if (!interrupted || CurrentProcess::TerminateReceived()) {
            break;
        }
    }

    return status;
}

void Process::Disown() {
    owner_ = false;
}

auto Process::WaitReady(PipeReader& pipe) -> void {
    auto read = pipe.Read<char>();
    if (!read) {
        throw std::system_error(read.error());
    }
}

auto Process::GetPid() const -> pid_t {
    return process_id_;
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

auto CurrentProcess::SignalReady() -> void {
    PipeWriter writer(3, false);
    auto written = writer.Write('\0');
    if (!written) {
        throw std::system_error(written.error());
    }
}

// void CurrentProcess::RequestTermination() {
//     terminate_sig_received_mut_.Lock();
//     terminate_sig_received_ = 1;
//     terminate_sig_received_mut_.Unlock();
// }

auto CurrentProcess::TerminateReceived() -> bool {
    // terminate_sig_received_mut_.Lock();
    auto ret = terminate_sig_received_ == 1;
    // terminate_sig_received_mut_.Unlock();
    return ret;
}

volatile sig_atomic_t CurrentProcess::terminate_sig_received_ = 0;
// ThreadMutex CurrentProcess::terminate_sig_received_mut_{};
