#pragma once
#include <sys/sem.h>

#include <cassert>
#include <cstring>
#include <expected>
#include <span>

#include "ipc/ipc.h"
#include "process.h"

union semun {
    int setval;                     // Value for SETVAL
    struct semid_ds* stat_set_buf;  // Buffer for IPC_STAT, IPC_SET
    unsigned short* get_set_array;  // Array for GETALL, SETALL
    struct seminfo* info_buf;       // Buffer for IPC_INFO (Linux-specific)
};

template <typename E>
    requires std::is_enum_v<E> && requires { E::COUNT; }
class SemaphoreSet {
  public:
    SemaphoreSet(SemaphoreSet&& other) noexcept
        : id_(other.id_), owner_(other.owner_) {
        other.owner_ = false;
    }
    auto operator=(SemaphoreSet&& other) = delete;
    // auto operator=(SemaphoreSet&& other) noexcept -> SemaphoreSet& {
    //     if (this == &other) {
    //         return *this;
    //     }
    //
    //     if (owner_) {
    //         auto removed = Remove();
    //     }
    //     id_ = other.id_;
    //     owner_ = other.owner_;
    //     other.ptr_ = nullptr;
    //     other.owner_ = false;
    //
    //     return *this;
    // };
    SemaphoreSet(const SemaphoreSet&) = delete;
    auto operator=(const SemaphoreSet&) -> SemaphoreSet& = delete;
    ~SemaphoreSet() {
        if (owner_) {
            auto removed = Remove();
        }
    }

    static auto Create(SemaphoreSetKey key,
                       std::initializer_list<unsigned short> init,
                       unsigned int permissions)
        -> std::expected<SemaphoreSet, IpcError> {
        std::span<const unsigned short> init_span(init.begin(), init.end());
        return Create(key, init_span, permissions);
    }

    [[nodiscard]]
    static auto Create(SemaphoreSetKey sem_key,
                       std::span<const unsigned short> init,
                       unsigned int permissions)
        -> std::expected<SemaphoreSet, IpcError> {
        auto key = static_cast<key_t>(sem_key);

        if (init.size() != static_cast<size_t>(E::COUNT)) {
            return std::unexpected(
                IpcError(IpcType::SEMAPHORE_SET, key, -1, EINVAL));
        }

        auto sem_id = GetSemId(sem_key, permissions | IPC_CREAT | IPC_EXCL);
        if (!sem_id) {
            return std::unexpected(
                IpcError(IpcType::SEMAPHORE_SET, key, -1, errno));
        }

        // NOLINTNEXTLINE(cppcoreguidelines-pro-type-const-cast)
        semun arg{.get_set_array = const_cast<unsigned short*>(init.data())};
        if (semctl(*sem_id, 0, SETALL, arg) == -1) {
            return std::unexpected(
                IpcError(IpcType::SEMAPHORE_SET, key, *sem_id, errno));
        }

        return SemaphoreSet(*sem_id, true);
    }

    [[nodiscard]]
    static auto Get(SemaphoreSetKey sem_key)
        -> std::expected<SemaphoreSet, IpcError> {
        auto key = static_cast<key_t>(sem_key);
        auto sem_id = GetSemId(sem_key, 0);
        if (!sem_id) {
            return std::unexpected(
                IpcError(IpcType::SEMAPHORE_SET, key, -1, errno));
        }
        return SemaphoreSet(*sem_id);
    }

    void Disown() {
        owner_ = false;
    }

    [[nodiscard]] auto Copy() const -> SemaphoreSet {
        return SemaphoreSet(id_, false);
    }

    [[nodiscard]]
    auto Remove() -> std::expected<void, IpcError> {
        if (owner_) {
            auto success = semctl(id_, 0, IPC_RMID);
            if (success == -1) {
                return std::unexpected(
                    IpcError(IpcType::SEMAPHORE_SET, -1, id_, errno));
            }
        }
        owner_ = false;

        return {};
    }

  private:
    explicit SemaphoreSet(int sem_id, bool owner = false)
        : id_(sem_id), owner_(owner) {};

    [[nodiscard]]
    static auto GetSemId(SemaphoreSetKey sem_key, unsigned int flags = 0)
        -> std::expected<int, IpcError> {
        auto key = static_cast<key_t>(sem_key);
        auto sem_id =
            semget(key, static_cast<int>(E::COUNT), static_cast<int>(flags));
        if (sem_id < 0) {
            return std::unexpected(
                IpcError(IpcType::SEMAPHORE_SET, key, -1, errno));
        }
        return sem_id;
    }

    int id_;
    bool owner_;

    friend class Semaphore;
};

class Semaphore {
  public:
    Semaphore(Semaphore&&) = default;
    Semaphore(const Semaphore&) = default;
    auto operator=(Semaphore&&) -> Semaphore& = default;
    auto operator=(const Semaphore&) -> Semaphore& = default;
    ~Semaphore() = default;

    template <typename E>
        requires std::is_enum_v<E> && requires { E::COUNT; }
    static auto Get(const SemaphoreSet<E>& semset, E sem_id) -> Semaphore {
        return Semaphore(semset.id_, static_cast<int>(sem_id));
    }

    [[nodiscard]]
    auto Wait() -> std::expected<void, IpcError> {
        return SemOp({
            .sem_num = sem_num_,
            .sem_op = -1,
            .sem_flg = 0,
        });
    }

    [[nodiscard]]
    auto WaitForZero() -> std::expected<void, IpcError> {
        return SemOp({
            .sem_num = sem_num_,
            .sem_op = 0,
            .sem_flg = 0,
        });
    }

    [[nodiscard]]
    auto Signal() -> std::expected<void, IpcError> {
        return SemOp({
            .sem_num = sem_num_,
            .sem_op = 1,
            .sem_flg = 0,
        });
    }

  private:
    Semaphore(int semset_id, unsigned short sem_num)
        : semset_id_(semset_id), sem_num_(sem_num) {}

    [[nodiscard]]
    auto SemOp(sembuf sop) const -> std::expected<void, IpcError> {
        while (true) {
            auto error = semop(semset_id_, &sop, 1);
            if (error == 0) {
                return {};
            }
            if (error != EINTR || CurrentProcess::TerminateReceived()) {
                return std::unexpected(
                    IpcError(IpcType::SEMAPHORE_SET, -1, semset_id_, errno));
            }
        }
        return {};
    }

    int semset_id_;
    unsigned short sem_num_;
};
