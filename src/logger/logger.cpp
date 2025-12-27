
#include "logger.h"

#include <fcntl.h>
#include <unistd.h>

#include <cstdio>
#include <experimental/scope>

#include "process.h"

namespace {
auto HandleExpectedError(const auto& expected) {
    if (!expected) {
        LogPrinter::PrintError("logger", expected.error().what());
    }
    return static_cast<bool>(expected);
}

constexpr void SetupSignals() {
    struct sigaction action{};
    action.sa_handler = SIG_IGN;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGINT, &action, nullptr);
}

}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    try {
        SetupSignals();
        auto log_printer = LogPrinter::Create();
        CurrentProcess::SignalReady();

        LogPrinter::Print("logger", Logger::LogLevel::INFO, "Listening...");
        auto success = log_printer.ReceiveForever();
        if (!HandleExpectedError(success)) {
            return 1;
        }

    } catch (std::exception& e) {
        LogPrinter::PrintError("logger", e.what());
        LogPrinter::Print("logger", Logger::LogLevel::INFO, "Goodbye");
        return 1;
    }

    LogPrinter::Print("logger", Logger::LogLevel::INFO, "Goodbye");
    return 0;
}
