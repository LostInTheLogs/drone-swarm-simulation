#pragma once

#include <sys/msg.h>

#include <cstdint>
#include <cstring>
#include <expected>
#include <system_error>

// NOLINTNEXTLINE(performance-enum-size)
enum class MsgQueueKey : key_t { MAIN };

// NOLINTNEXTLINE(performance-enum-size)
enum class MessageTypeId : int { LOGGER };

enum class IpcType : uint8_t { MESSAGE_QUEUE, SEMAPHORE, SHARED_MEMORY };

class IpcError : public std::system_error {
  public:
    explicit IpcError(IpcType ipc_type, key_t key, int ipc_id, int error);

  private:
    static auto IpcTypeToStr(IpcType ipc_type) -> const char *;
};

class IpcMessageQueue {
  public:
    IpcMessageQueue(IpcMessageQueue &&) = default;
    IpcMessageQueue(const IpcMessageQueue &) = delete;
    auto operator=(IpcMessageQueue &&) -> IpcMessageQueue & = default;
    auto operator=(const IpcMessageQueue &) -> IpcMessageQueue & = delete;
    ~IpcMessageQueue() = default;

    [[nodiscard]]
    static auto Create(MsgQueueKey queue_key, unsigned int permissions)
        -> std::expected<IpcMessageQueue, IpcError>;
    [[nodiscard]]
    static auto Get(MsgQueueKey queue_key)
        -> std::expected<IpcMessageQueue, IpcError>;

    [[nodiscard]]
    auto Remove() const -> std::expected<void, IpcError>;

    template <typename PayloadType>
    [[nodiscard]]
    auto Send(PayloadType payload, long type, bool wait = true) const
        -> std::expected<void, IpcError> {
        static_assert(std::is_trivially_copyable_v<PayloadType>,
                      "Payload must be trivially copyable");

        const Message<PayloadType> msg{.type = type, .payload = payload};

        const auto flags = (wait ? 0U : IPC_NOWAIT);

        int result = 0;
        while (true) {
            result = msgsnd(queue_id_, &msg, sizeof(PayloadType),
                            static_cast<int>(flags));
            const auto interrupted = result == -1 && errno == EINTR;
            if (!interrupted) {
                break;
            }
        }

        if (result == -1) {
            return std::unexpected(
                IpcError(IpcType::MESSAGE_QUEUE, key_, queue_id_, errno));
        }

        return {};
    }

    template <typename PayloadType>
    [[nodiscard]]
    auto Receive(long type, bool wait = true) const
        -> std::expected<PayloadType, IpcError> {
        static_assert(std::is_trivially_copyable_v<PayloadType>,
                      "Payload must be trivially copyable");

        Message<PayloadType> msg;

        const auto flags = (wait ? 0U : IPC_NOWAIT);

        int result = 0;

        while (true) {
            result = msgrcv(queue_id_, &msg, sizeof(PayloadType), type,
                            static_cast<int>(flags));
            const auto interrupted = result == -1 && errno == EINTR;
            if (!interrupted) {
                break;
            }
        }

        if (result == -1) {
            return std::unexpected(
                IpcError(IpcType::MESSAGE_QUEUE, key_, queue_id_, errno));
        }

        return msg.payload;
    }

  private:
    explicit IpcMessageQueue(key_t key, int queue_id, bool owner = false);

    template <typename PayloadType>
    struct Message {
        long type = 0;  // must be > 0
        PayloadType payload;
    };

    key_t key_;
    int queue_id_;
    bool owner_;
};
