
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
}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    auto log_receiver = LogPrinter::Create();
    if (!HandleExpectedError(log_receiver)) {
        return 1;
    }

    if (!CurrentProcess::SignalReady()) {
        return 1;
    }

    auto success = log_receiver->ReceiveForever();
    if (!HandleExpectedError(success)) {
        return 1;
    }

    return 0;
}
