#include <unistd.h>

#include <csignal>
#include <experimental/scope>

#include "logger.h"
#include "process.h"

namespace {
auto HandleExpectedError(const auto& expected) {
    if (!expected) {
        LogPrinter::PrintError("logger", expected.error().what());
    }
    return static_cast<bool>(expected);
}
}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    auto logger_process = Process::CreateReady({"./logger"});
    if (!HandleExpectedError(logger_process)) {
        return 1;
    }

    auto logger = Logger::Create("main");
    if (!HandleExpectedError(logger)) {
        return 1;
    }

    logger->Debug("test");
    logger->Info("test");
    logger->Warning("test");
    logger->Error("test");
    sleep(1);

    auto joined = logger_process->TermWait();
    if (!HandleExpectedError(joined)) {
        return 1;
    }
    return 0;
}
