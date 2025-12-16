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

    PayloadSenderT name_;
    IpcMessageQueue queue_;

    friend class LogPrinter;
};

class LogPrinter {
  public:
    static auto Create() -> std::expected<LogPrinter, IpcError>;
    auto ReceiveForever() -> std::expected<void, IpcError>;

    static void PrintError(std::string_view sender, std::string_view msg);

  private:
    static auto FormatLog(Logger::Payload log) -> std::string;
    static auto LogLevelToStr(Logger::LogLevel level) -> std::string;

    explicit LogPrinter(IpcMessageQueue queue);
    IpcMessageQueue queue_;
};
