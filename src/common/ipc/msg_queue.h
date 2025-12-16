#pragma once

#include <sys/msg.h>

#include <cstring>
#include <expected>

#include "ipc/ipc.h"
#include "process.h"

class IpcMessageQueue {
  public:
    IpcMessageQueue(IpcMessageQueue&&) noexcept;
    auto operator=(IpcMessageQueue&&) noexcept -> IpcMessageQueue&;
    IpcMessageQueue(const IpcMessageQueue&) = delete;
    auto operator=(const IpcMessageQueue&) -> IpcMessageQueue& = delete;
    ~IpcMessageQueue();

    [[nodiscard]]
    static auto Create(MsgQueueKey queue_key, unsigned int permissions)
        -> std::expected<IpcMessageQueue, IpcError>;

    [[nodiscard]]
    static auto GetOrCreate(MsgQueueKey queue_key, unsigned int permissions,
                            bool owner = false)
        -> std::expected<IpcMessageQueue, IpcError>;

    [[nodiscard]]
    static auto Get(MsgQueueKey queue_key)
        -> std::expected<IpcMessageQueue, IpcError>;

    [[nodiscard]] auto Copy() const -> IpcMessageQueue;

    [[nodiscard]]
    auto Remove() -> std::expected<void, IpcError>;

    template <typename PayloadType>
    [[nodiscard]]
    auto Send(PayloadType payload, MessageTypeId type, bool wait = true) const
        -> std::expected<void, IpcError> {
        static_assert(std::is_trivially_copyable_v<PayloadType>,
                      "Payload must be trivially copyable");

        const Message<PayloadType> msg{.type = static_cast<long>(type),
                                       .payload = payload};

        const auto flags = (wait ? 0U : IPC_NOWAIT);

        int result = 0;
        while (CurrentProcess::Get().terminate_sig_received != 1) {
            result =
                msgsnd(id_, &msg, sizeof(PayloadType), static_cast<int>(flags));
            const auto interrupted = result == -1 && errno == EINTR;
            if (!interrupted) {
                break;
            }
        }

        if (result == -1) {
            return std::unexpected(
                IpcError(IpcType::MESSAGE_QUEUE, -1, id_, errno));
        }

        return {};
    }

    template <typename PayloadType>
    [[nodiscard]]
    auto Receive(MessageTypeId type, bool wait = true) const
        -> std::expected<PayloadType, IpcError> {
        static_assert(std::is_trivially_copyable_v<PayloadType>,
                      "Payload must be trivially copyable");

        Message<PayloadType> msg;

        const auto flags = (wait ? 0U : IPC_NOWAIT);

        int result = 0;

        while (CurrentProcess::Get().terminate_sig_received != 1) {
            result = msgrcv(id_, &msg, sizeof(PayloadType),
                            static_cast<long>(type), static_cast<int>(flags));
            const auto interrupted = result == -1 && errno == EINTR;
            if (!interrupted) {
                break;
            }
        }

        if (result == -1) {
            return std::unexpected(
                IpcError(IpcType::MESSAGE_QUEUE, -1, id_, errno));
        }

        return msg.payload;
    }

  private:
    explicit IpcMessageQueue(int queue_id, bool owner = false);

    template <typename PayloadType>
    struct Message {
        long type = 0;  // must be > 0
        PayloadType payload;
    };

    [[nodiscard]]
    static auto GetQueueId(MsgQueueKey queue_key, unsigned int flags = 0)
        -> std::expected<int, IpcError>;

    int id_;
    bool owner_;
};
