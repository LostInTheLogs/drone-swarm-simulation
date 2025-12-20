#include "thread.h"

#include <pthread.h>

auto Thread::Create(const std::function<void()>& function)
    -> std::expected<Thread, std::system_error> {
    Thread thread;

    auto* heap_fn = new std::function(function);

    auto error = pthread_create(
        &thread.thread_id_, nullptr,
        [](void* void_callable) -> void* {
            auto* callable = static_cast<Callable*>(void_callable);
            (*callable)();
            delete callable;
            return nullptr;
        },
        heap_fn);

    if (error != 0) {
        return std::unexpected(std::system_error());
    }

    return thread;
}

auto Thread::Join() const -> std::expected<void, std::system_error> {
    auto error = pthread_join(thread_id_, nullptr);
    if (error != 0) {
        return std::unexpected(
            std::system_error(error, std::generic_category()));
    }

    return {};
}

auto Thread::Cancel() const -> std::expected<void, std::system_error> {
    auto error = pthread_cancel(thread_id_);
    if (error != 0) {
        return std::unexpected(
            std::system_error(error, std::generic_category()));
    }

    return {};
}

void ThreadMutex::Lock() {
    pthread_mutex_lock(&mutex_);
}
void ThreadMutex::Unlock() {
    pthread_mutex_unlock(&mutex_);
}
