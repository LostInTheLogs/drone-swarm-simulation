#pragma once

#include <cstdint>
#include <system_error>

// NOLINTNEXTLINE(performance-enum-size)
enum class MsgQueueKey : key_t { MAIN = 33889 };

// NOLINTNEXTLINE(performance-enum-size)
enum class MessageTypeId : long { LOGGER = 1 };

// NOLINTNEXTLINE(performance-enum-size)
enum class SemaphoreSetKey : key_t { MAIN = 33889 };

// NOLINTNEXTLINE(performance-enum-size)
enum class SharedMemoryKey : key_t { MAIN = 33890 };

// NOLINTNEXTLINE(performance-enum-size)
enum class SemaphoreId : int { GRACEFUL_EXIT, SEMAPHORE_NUM };

enum class IpcType : uint8_t { MESSAGE_QUEUE, SEMAPHORE_SET, SHARED_MEMORY };

class IpcError : public std::system_error {
  public:
    explicit IpcError(IpcType ipc_type, key_t key, int ipc_id, int error);

  private:
    static auto IpcTypeToStr(IpcType ipc_type) -> const char*;
};
