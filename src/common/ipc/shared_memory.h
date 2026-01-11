#pragma once
#include <sys/shm.h>

#include <cstring>
#include <expected>

#include "ipc/ipc.h"

template <typename T>
class SharedMemory {
  public:
    using value_type = T;

    SharedMemory(SharedMemory&& other) noexcept
        : id_(other.id_), ptr_(other.ptr_), owner_(other.owner_) {
        other.owner_ = false;
        other.ptr_ = nullptr;
    }
    auto operator=(SharedMemory&& other) = delete;
    SharedMemory(const SharedMemory&) = delete;
    auto operator=(const SharedMemory&) -> SharedMemory& = delete;
    ~SharedMemory() {
        try {
            if (ptr_ != nullptr) {
                Detach();
            }
            if (owner_) {
                Remove();
            }
        } catch (const std::system_error&) {  // NOLINT
        }
    }

    [[nodiscard]]
    static auto Create(ShmKey queue_key, unsigned int permissions,
                       size_t size = sizeof(T)) -> SharedMemory {
        auto mem_id =
            GetMemId(queue_key, permissions | IPC_CREAT | IPC_EXCL, size);
        auto ret = SharedMemory(mem_id, true);

        ret.Attach();

        new (ret.ptr_) T();

        return std::move(ret);
    }

    [[nodiscard]]
    static auto Get(ShmKey queue_key, size_t size = sizeof(T)) -> SharedMemory {
        auto mem_id = GetMemId(queue_key, 0, size);
        auto ret = SharedMemory(mem_id);
        ret.Attach();
        return std::move(ret);
    }

    void Disown() {
        owner_ = false;
    }

    [[nodiscard]] auto Copy() const -> SharedMemory {
        return SharedMemory(id_, false, ptr_);
    }

    void Remove() {
        auto success = shmctl(id_, IPC_RMID, nullptr);
        if (success == -1) {
            throw IpcError(IpcType::SHARED_MEMORY, -1, id_, errno);
        }
        owner_ = false;
    }

    void Attach() {
        auto* mem = shmat(id_, nullptr, 0);
        if (reinterpret_cast<std::intptr_t>(mem) == -1) {
            throw IpcError(IpcType::SHARED_MEMORY, -1, id_, errno);
        }
        ptr_ = mem;
    }

    void Detach() {
        if (shmdt(ptr_) == -1) {
            throw IpcError(IpcType::SHARED_MEMORY, -1, id_, errno);
        }
        ptr_ = nullptr;
    }

    auto operator*() -> T& {
        return *reinterpret_cast<T*>(ptr_);
    }

    auto operator->() -> T* {
        return reinterpret_cast<T*>(ptr_);
    }

    auto operator->() const -> const T* {
        return reinterpret_cast<T*>(ptr_);
    }

  private:
    explicit SharedMemory(int queue_id, bool owner = false, T* ptr = nullptr)
        : id_(queue_id), ptr_(ptr), owner_(owner) {};

    [[nodiscard]]
    static auto GetMemId(ShmKey queue_key, unsigned int flags, size_t size)
        -> int {
        auto key = static_cast<key_t>(queue_key);
        auto mem_id = shmget(key, size, static_cast<int>(flags));
        if (mem_id < 0) {
            throw IpcError(IpcType::SHARED_MEMORY, key, -1, errno);
        }
        return mem_id;
    }

    int id_;
    void* ptr_{};
    bool owner_;
};
