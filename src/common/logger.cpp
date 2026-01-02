#include "logger.h"

#include <fcntl.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <csignal>
#include <format>
#include <iostream>
#include <iterator>
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

void Logger::Trace(string_view msg) {
    Logger::Log(LogLevel::TRACE, msg);
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
    static auto queue = IpcMessageQueue::Create(MsgQueueKey::MAIN, 0600);
    return LogPrinter(std::move(queue));
}

auto LogPrinter::FormatLog(Logger::Payload log, bool colored) -> std::string {
    string_view msg(log.msg.data());
    string_view sender(log.sender_name.data());
    const auto level = LogLevelToStr(log.level, colored);
    const auto local_time =
        std::chrono::zoned_time(std::chrono::current_zone(), log.time);
    const auto color = colored ? LogLevelToColor(log.level) : "";

    const auto* const clear_color = colored ? "\033[0m" : "";
    const auto col_msg = format("{}{}{}", color, msg, clear_color);

    return format("[{:%F %T}] {:>5} {}({}): {}\n", local_time, level, sender,
                  log.sender_pid, col_msg);
}

auto LogPrinter::LogLevelToColor(Logger::LogLevel level) -> std::string {
    switch (level) {
        case Logger::TRACE:
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

auto LogPrinter::LogLevelToStr(Logger::LogLevel level, bool colored)
    -> std::string {
    if (colored) {
        switch (level) {
            case Logger::TRACE:
                return "\033[1;37mTRACE\033[0m";
            case Logger::DEBUG:
                return "\033[1;37mDEBUG\033[0m";
            case Logger::INFO:
                return "\033[1;36mINFO \033[0m";
            case Logger::WARNING:
                return "\033[1;33mWARN \033[0m";
            case Logger::ERROR:
                return "\033[1;31mERROR\033[0m";
        }
    }
    switch (level) {
        case Logger::TRACE:
            return "TRACE";
        case Logger::DEBUG:
            return "DEBUG";
        case Logger::INFO:
            return "INFO ";
        case Logger::WARNING:
            return "WARN ";
        case Logger::ERROR:
            return "ERROR";
    }
    return {};
}

void LogPrinter::ReceiveForever() {
    const auto file = open("./logs.txt", O_CREAT | O_WRONLY | O_TRUNC, 0600);
    if (file == -1) {
        throw std::system_error(errno, std::generic_category());
    }

    while (true) {
        auto message = queue_.Receive<Logger::Payload>(MessageTypeId::LOGGER);
        if (!message) {
            if (message.error().code() == std::errc::interrupted) {
                if (-1 == close(file)) {
                    throw std::system_error(errno, std::generic_category());
                }
                return;
            }
            throw IpcError(message.error());
        }
        const auto formatted_col = FormatLog(*message);
        std::cout << formatted_col;

        const auto formatted = FormatLog(*message, false);
        if (-1 == write(file, formatted.c_str(), formatted.length())) {
            if (-1 == close(file)) {
                perror("Couldn't close file when write() failed");
            }
            throw std::system_error(errno, std::generic_category());
        }
    }
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
