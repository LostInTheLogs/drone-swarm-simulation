#include "process.h"

#include <pthread.h>
#include <sys/wait.h>
#include <unistd.h>

#include <csignal>
#include <system_error>

Process::Process(pid_t process_id) : process_id_(process_id) {}

auto Process::Create(std::span<const char*> args)
    -> std::expected<Process, std::system_error> {
    auto process_id = fork();

    if (process_id == 0) {
        // close all but first 3 file descriptors
        auto max_fd = sysconf(_SC_OPEN_MAX);
        if (max_fd == -1) {
            max_fd = 1024;  // fallback NOLINT(readability-magic-numbers)
        }
        for (int fd = 3; fd < max_fd; ++fd) {
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
    } else if (process_id == -1) {
        return std::unexpected(
            std::system_error(errno, std::generic_category()));
    }

    return Process(process_id);
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
