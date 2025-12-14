#include "logger.h"

#include <algorithm>
#include <format>
#include <iostream>
#include <utility>

using std::expected, std::unexpected, std::string_view;

namespace {

template <size_t N>
constexpr void CopyStrToArray(string_view str, std::array<char, N>& array) {
    const size_t len = (str.size() < N - 1) ? str.size() : (N - 1);
    std::copy_n(str.begin(), len, array.begin());
    array.at(len) = '\0';
}
};  // namespace

Logger::Logger(string_view name, IpcMessageQueue queue)
    : name_(), queue_(std::move(queue)) {
    CopyStrToArray(name, name_);
}

auto Logger::Create(string_view name) -> expected<Logger, IpcError> {
    static auto message_queue = IpcMessageQueue::Get(MsgQueueKey::MAIN).value();
    return Logger(name, message_queue.Copy());
}

void Logger::PrintLog(Payload log) {
    string_view msg(log.msg);
    string_view sender(log.sender);
    auto level = LogLevelToStr(log.level);

    std::cout << format("[{:>5}] {}: {}", level, sender, msg) << '\n';
}

void Logger::Log(LogLevel level, string_view msg) {
    Payload payload{.level = level, .sender = name_, .msg = {}};

    CopyStrToArray(msg, payload.msg);

    queue_.Send(payload, MessageTypeId::LOGGER).value();
}

void Logger::Debug(string_view msg) {
    Logger::Log(LogLevel::DEBUG, msg);
}
void Logger::Info(string_view msg) {
    Logger::Log(LogLevel::INFO, msg);
}
void Logger::Warning(string_view msg) {
    Logger::Log(LogLevel::WARNING, msg);
}
void Logger::Error(string_view msg) {
    Logger::Log(LogLevel::ERROR, msg);
}

auto Logger::LogLevelToStr(Logger::LogLevel level) -> std::string {
    switch (level) {
        case DEBUG:
            return "DEBUG";
        case INFO:
            return "INFO";
        case WARNING:
            return "WARN";
        case ERROR:
            return "ERROR";
    }
    return {};
}

LogReceiver::LogReceiver(IpcMessageQueue queue) : queue_(std::move(queue)) {}

auto LogReceiver::Create() -> expected<LogReceiver, IpcError> {
    static auto queue = IpcMessageQueue::Create(MsgQueueKey::MAIN, 0666);
    if (!queue) {
        return unexpected(queue.error());
    }

    return LogReceiver(std::move(*queue));
}
auto LogReceiver::ReceiveForever() -> expected<void, IpcError> {
    while (true) {
        auto message = queue_.Receive<Logger::Payload>(MessageTypeId::LOGGER);
        if (!message) {
            return unexpected(message.error());
        }
        Logger::PrintLog(*message);
    }
}
