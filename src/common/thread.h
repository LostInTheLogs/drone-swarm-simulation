#pragma once

#include <pthread.h>

#include <chrono>
#include <expected>
#include <functional>
#include <system_error>

#include "process.h"

class Thread {
  private:
    using Callable = std::function<void()>;

  public:
    [[nodiscard]] static auto Create(const Callable &function)
        -> std::expected<Thread, std::system_error>;
    [[nodiscard]] auto Join() const -> std::expected<void, std::system_error>;
    [[nodiscard]] auto Cancel() const -> std::expected<void, std::system_error>;

    template <class Clock, class Duration>
    static auto SleepUntil(
        const std::chrono::time_point<Clock, Duration> &until)
        -> std::expected<void, std::system_error> {
        using std::chrono::time_point, std::chrono::nanoseconds,
            std::chrono::seconds;

        auto until_ns = time_point_cast<nanoseconds>(until).time_since_epoch();

        auto sec = duration_cast<seconds>(until_ns);
        auto nsec = until_ns - sec;

        timespec tspec{.tv_sec = sec.count(), .tv_nsec = nsec.count()};

        while (true) {
            auto error = clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &tspec,
                                         nullptr);
            if (error == 0) {
                return {};
            }
            if (error != EINTR || CurrentProcess::TerminateReceived()) {
                return std::unexpected(
                    std::system_error(error, std::generic_category()));
            }
        }

        return {};
    }

  private:
    pthread_t thread_id_{};
};

class ThreadMutex {
  public:
    ThreadMutex() = default;
    ThreadMutex(ThreadMutex &&) = delete;
    ThreadMutex(const ThreadMutex &) = delete;
    auto operator=(ThreadMutex &&) = delete;
    auto operator=(const ThreadMutex &) -> ThreadMutex & = delete;
    ~ThreadMutex() = default;

    void Lock();
    void Unlock();

  private:
    pthread_mutex_t mutex_ = PTHREAD_MUTEX_INITIALIZER;
};
