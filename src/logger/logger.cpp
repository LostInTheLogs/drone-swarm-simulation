#include "logger.h"

#include "globals.h"
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

        auto params = ShmParameters::Get(ShmKey::PARAMS);
        auto log_printer = LogPrinter::Create(params->log_level);
        params.Detach();

        CurrentProcess::SignalReady();

        LogPrinter::Print("logger", Logger::LogLevel::INFO, "Listening...");
        log_printer.ReceiveForever();

    } catch (std::exception& e) {
        LogPrinter::PrintError("logger", e.what());
        LogPrinter::Print("logger", Logger::LogLevel::INFO, "Goodbye");
        return 1;
    }

    LogPrinter::Print("logger", Logger::LogLevel::INFO, "Goodbye");
    return 0;
}
