#pragma once

#include <cstdint>
#include <system_error>

constexpr key_t g_rand_key = 33889;

// NOLINTNEXTLINE(performance-enum-size)
enum class MsgQueueKey : key_t { MAIN = g_rand_key };

// NOLINTNEXTLINE(performance-enum-size)
enum class MessageTypeId : long { LOGGER = 1 };

// NOLINTNEXTLINE(performance-enum-size)
enum class SemSetKey : key_t { MAIN = g_rand_key };

// NOLINTNEXTLINE(performance-enum-size)
enum class ShmKey : key_t { PARAMS = g_rand_key, IN_QUEUE, OUT_QUEUE };

// NOLINTNEXTLINE(performance-enum-size)
// enum class TestSem : int { GRACEFUL_EXIT, COUNT };

enum class IpcType : uint8_t { MESSAGE_QUEUE, SEMAPHORE_SET, SHARED_MEMORY };

class IpcError : public std::system_error {
  public:
    explicit IpcError(IpcType ipc_type, key_t key, int ipc_id, int error);

  private:
    static auto IpcTypeToStr(IpcType ipc_type) -> const char*;
};
