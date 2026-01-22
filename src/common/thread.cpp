#include "thread.h"

#include <pthread.h>

#include <exception>

Thread::Thread(Thread&& other) noexcept
    : thread_id_(other.thread_id_), joinable_(other.joinable_) {
    other.thread_id_ = 0;
    other.joinable_ = false;
}

auto Thread::operator=(Thread&& other) noexcept -> Thread& {
    if (this == &other) {
        return *this;
    }

    if (joinable_) {
        try {
            Detach();
        } catch (const std::system_error&) {
            std::terminate();
        }
    }

    thread_id_ = other.thread_id_;
    other.thread_id_ = 0;

    joinable_ = other.joinable_;
    other.joinable_ = false;
    return *this;
}

Thread::~Thread() {
    if (joinable_) {
        std::terminate();
    }
}

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
        throw std::system_error(error, std::generic_category());
    }

    return thread;
}

void Thread::Join() {
    if (!joinable_) {
        return;
    }

    auto error = pthread_join(thread_id_, nullptr);
    if (error != 0) {
        throw std::system_error(error, std::generic_category());
    }

    joinable_ = false;
}

void Thread::Cancel() const {
    auto error = pthread_cancel(thread_id_);
    if (error != 0) {
        throw std::system_error(error, std::generic_category());
    }
}

void Thread::Detach() {
    auto error = pthread_detach(thread_id_);
    if (error != 0) {
        throw std::system_error(error, std::generic_category());
    }

    joinable_ = false;
}
