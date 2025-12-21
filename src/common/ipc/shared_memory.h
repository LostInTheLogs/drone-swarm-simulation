#pragma once
#include <sys/shm.h>

#include <cstring>
#include <expected>

#include "ipc/ipc.h"

template <typename T>
class SharedMemory {
  public:
    SharedMemory(SharedMemory&& other) noexcept
        : id_(other.id_), ptr_(other.ptr_), owner_(other.owner_) {
        other.owner_ = false;
        other.ptr_ = nullptr;
    }
    auto operator=(SharedMemory&& other) = delete;
    // auto operator=(SharedMemory&& other) noexcept -> SharedMemory& {
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
    SharedMemory(const SharedMemory&) = delete;
    auto operator=(const SharedMemory&) -> SharedMemory& = delete;
    ~SharedMemory() {
        if (ptr_) {
            auto detached = Detach();
        }
        if (owner_) {
            auto removed = Remove();
        }
    }

    [[nodiscard]]
    static auto Create(SharedMemoryKey queue_key, unsigned int permissions)
        -> std::expected<SharedMemory, IpcError> {
        auto key = static_cast<key_t>(queue_key);
        auto mem_id = GetMemId(queue_key, permissions | IPC_CREAT | IPC_EXCL);
        if (!mem_id) {
            return std::unexpected(
                IpcError(IpcType::SHARED_MEMORY, key, -1, errno));
        }
        auto ret = SharedMemory(*mem_id, true);

        auto atached = ret.Attach();
        if (!atached) {
            return std::unexpected(atached.error());
        }

        T init{};
        memcpy(ret.ptr_, &init, sizeof(T));

        return std::move(ret);
    }

    [[nodiscard]]
    static auto Get(SharedMemoryKey queue_key)
        -> std::expected<SharedMemory, IpcError> {
        auto key = static_cast<key_t>(queue_key);
        auto mem_id = GetMemId(queue_key, 0);
        if (!mem_id) {
            return std::unexpected(
                IpcError(IpcType::SHARED_MEMORY, key, -1, errno));
        }
        return SharedMemory(*mem_id);
    }

    void Disown() {
        owner_ = false;
    }

    [[nodiscard]] auto Copy() const -> SharedMemory {
        return SharedMemory(id_, false, ptr_);
    }

    [[nodiscard]]
    auto Remove() -> std::expected<void, IpcError> {
        if (owner_) {
            auto success = shmctl(id_, IPC_RMID, nullptr);
            if (success == -1) {
                return std::unexpected(
                    IpcError(IpcType::SHARED_MEMORY, -1, id_, errno));
            }
        }
        owner_ = false;

        return {};
    }

    auto operator->() -> T* {
        return ptr_;
    }

    auto operator->() const -> const T* {
        return ptr_;
    }

  private:
    explicit SharedMemory(int queue_id, bool owner = false, T* ptr = nullptr)
        : id_(queue_id), ptr_(ptr), owner_(owner) {};

    [[nodiscard]]
    static auto GetMemId(SharedMemoryKey queue_key, unsigned int flags = 0)
        -> std::expected<int, IpcError> {
        auto key = static_cast<key_t>(queue_key);
        auto mem_id = shmget(key, sizeof(T), static_cast<int>(flags));
        if (mem_id < 0) {
            return std::unexpected(
                IpcError(IpcType::SHARED_MEMORY, key, -1, errno));
        }
        return mem_id;
    }

    [[nodiscard]]
    auto Attach() -> std::expected<void, IpcError> {
        auto* mem = shmat(id_, nullptr, 0);
        if (reinterpret_cast<std::intptr_t>(mem) == -1) {
            return std::unexpected(
                IpcError(IpcType::SHARED_MEMORY, -1, id_, errno));
        }
        ptr_ = reinterpret_cast<T*>(mem);
        return {};
    }

    [[nodiscard]]
    auto Detach() -> std::expected<void, IpcError> {
        if (shmdt(ptr_) == -1) {
            return std::unexpected(
                IpcError(IpcType::SHARED_MEMORY, -1, id_, errno));
        }
        return {};
    }

    int id_;
    T* ptr_{};
    bool owner_;
};
