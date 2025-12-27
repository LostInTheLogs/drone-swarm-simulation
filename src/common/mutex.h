#pragma once

#include "ipc/ipc.h"
#include "ipc/semaphore_set.h"

#define RWMUT_SEMS(P) SemIds::P##_A0, SemIds::P##_B1, SemIds::P##_C1

class RWMutex {
  public:
    template <auto A0, auto B1, auto C1>
    static auto Get(const SemaphoreSet<std::remove_cvref_t<decltype(A0)>> &sems)
        -> RWMutex {
        using E = std::remove_cvref_t<decltype(A0)>;
        static_assert(SemInit<E>::Get()[static_cast<size_t>(A0)] == 0);
        static_assert(SemInit<E>::Get()[static_cast<size_t>(B1)] == 1);
        static_assert(SemInit<E>::Get()[static_cast<size_t>(C1)] == 1);
        return RWMutex(Semaphore::Get(sems, A0), Semaphore::Get(sems, B1),
                       Semaphore::Get(sems, C1));
    }

    RWMutex(RWMutex &&) = default;
    RWMutex(const RWMutex &) = default;
    auto operator=(RWMutex &&) -> RWMutex & = default;
    auto operator=(const RWMutex &) -> RWMutex & = default;
    ~RWMutex() = default;

    auto LockRead(Retry retry = Retry::UNTIL_TERM)
        -> std::expected<void, IpcError>;
    void UnlockRead(Retry retry = Retry::UNTIL_TERM);
    auto LockWrite(Retry retry = Retry::UNTIL_TERM)
        -> std::expected<void, IpcError>;
    void UnlockWrite(Retry retry = Retry::UNTIL_TERM);

  private:
    RWMutex(Semaphore reader_count, Semaphore reader_count_mut,
            Semaphore writer_mut);
    Semaphore reader_count_;
    Semaphore reader_count_mut_;
    Semaphore writer_mut_;
};
