#pragma once

#include <array>
#include <expected>

#include "ipc/msg_queue.h"

class Logger {
  public:
    enum LogLevel : uint8_t { DEBUG, INFO, WARNING, ERROR };

    static auto Create(std::string_view name)
        -> std::expected<Logger, IpcError>;

    void Log(LogLevel level, std::string_view msg);
    void Debug(std::string_view msg);
    void Info(std::string_view msg);
    void Warning(std::string_view msg);
    void Error(std::string_view msg);

  private:
    using PayloadSenderT =
        std::array<char, 32>;  // NOLINT(readability-magic-numbers)
    using PayloadMsgT =
        std::array<char, 256>;  // NOLINT(readability-magic-numbers)

    struct Payload {
        LogLevel level;
        PayloadSenderT sender;
        PayloadMsgT msg;
    };

    explicit Logger(std::string_view name, IpcMessageQueue queue);

    static void PrintLog(Payload log);
    static auto LogLevelToStr(LogLevel level) -> std::string;

    PayloadSenderT name_;
    IpcMessageQueue queue_;

    friend class LogReceiver;
};

class LogReceiver {
  public:
    static auto Create() -> std::expected<LogReceiver, IpcError>;
    auto ReceiveForever() -> std::expected<void, IpcError>;

  private:
    explicit LogReceiver(IpcMessageQueue queue);
    IpcMessageQueue queue_;
};
