#include "thread.h"

#include <pthread.h>

auto Thread::Create(const std::function<void()>& function) -> Thread {
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
        throw std::system_error();
    }

    return thread;
}

void Thread::Join() const {
    auto error = pthread_join(thread_id_, nullptr);
    if (error != 0) {
        throw std::system_error(error, std::generic_category());
    }
}

void Thread::Cancel() const {
    auto error = pthread_cancel(thread_id_);
    if (error != 0) {
        throw std::system_error(error, std::generic_category());
    }
}
