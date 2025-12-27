#include "logger.h"

#include <unistd.h>

#include <algorithm>
#include <chrono>
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

auto Logger::Create(string_view name) -> Logger {
    static auto queue = IpcMessageQueue::Get(MsgQueueKey::MAIN);
    return Logger(name, queue.Copy());
}

void Logger::Log(LogLevel level, string_view msg) {
    Payload payload{.level = level,
                    .sender_pid = getpid(),
                    .sender_name = name_,
                    .msg = {},
                    .time = std::chrono::system_clock::now()};

    CopyStrToArray(msg, payload.msg);

    auto sent = queue_.Send(payload, MessageTypeId::LOGGER);
    if (!sent) {
        LogPrinter::PrintError(
            string_view(name_),
            std::format("Sending logs failed: {}", sent.error().what()));
    }
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

LogPrinter::LogPrinter(IpcMessageQueue queue) : queue_(std::move(queue)) {}

auto LogPrinter::Create() -> LogPrinter {
    static auto queue = IpcMessageQueue::Create(MsgQueueKey::MAIN, 0666);
    return LogPrinter(std::move(queue));
}

auto LogPrinter::FormatLog(Logger::Payload log) -> std::string {
    string_view msg(log.msg);
    string_view sender(log.sender_name);
    const auto level = LogLevelToStr(log.level);
    const auto local_time =
        std::chrono::zoned_time(std::chrono::current_zone(), log.time);
    const auto color = LogLevelToColor(log.level);

    const auto* const clear_color = "\033[0m";
    const auto col_msg = format("{}{}{}", color, msg, clear_color);

    return format("[{:%F %T}] {:>5} {}({}): {}\n", local_time, level, sender,
                  log.sender_pid, col_msg);
}

auto LogPrinter::LogLevelToColor(Logger::LogLevel level) -> std::string {
    switch (level) {
        case Logger::DEBUG:
            return "\033[37m";
        case Logger::INFO:
            return "\033[36m";
        case Logger::WARNING:
            return "\033[33m";
        case Logger::ERROR:
            return "\033[31m";
    }
    return {};
}

auto LogPrinter::LogLevelToStr(Logger::LogLevel level) -> std::string {
    switch (level) {
        case Logger::DEBUG:
            return "\033[1;37mDEBUG\033[0m";
        case Logger::INFO:
            return "\033[1;36mINFO \033[0m";
        case Logger::WARNING:
            return "\033[1;33mWARN \033[0m";
        case Logger::ERROR:
            return "\033[1;31mERROR\033[0m";
    }
    return {};
}

auto LogPrinter::ReceiveForever() -> expected<void, IpcError> {
    while (true) {
        auto message = queue_.Receive<Logger::Payload>(MessageTypeId::LOGGER);
        if (!message) {
            if (message.error().code() == std::errc::interrupted) {
                return {};
            }
            return unexpected(message.error());
        }
        const auto formatted = FormatLog(*message);
        std::cout << formatted;
    }

    return {};
}

void LogPrinter::PrintError(std::string_view sender, std::string_view msg) {
    Logger::Payload payload{.level = Logger::ERROR,
                            .sender_pid = getpid(),
                            .sender_name = {},
                            .msg = {},
                            .time = std::chrono::system_clock::now()};
    CopyStrToArray(msg, payload.msg);
    CopyStrToArray(sender, payload.sender_name);
    const auto formatted = FormatLog(payload);
    std::cerr << formatted;
}
void LogPrinter::Print(std::string_view sender, Logger::LogLevel level,
                       std::string_view msg) {
    Logger::Payload payload{.level = level,
                            .sender_pid = getpid(),
                            .sender_name = {},
                            .msg = {},
                            .time = std::chrono::system_clock::now()};
    CopyStrToArray(msg, payload.msg);
    CopyStrToArray(sender, payload.sender_name);
    const auto formatted = FormatLog(payload);
    std::cout << formatted;
}
