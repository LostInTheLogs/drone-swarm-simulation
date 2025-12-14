#include <unistd.h>

#include <experimental/scope>
#include <iostream>

#include "logger.h"
#include "thread.h"

namespace {
auto HandleExpected(const auto& expected) {
    if (!expected) {
        std::cout << expected.error().what() << '\n';
    }
    return static_cast<bool>(expected);
}
}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    auto log_receiver = LogReceiver::Create();
    if (!HandleExpected(log_receiver)) {
        return 1;
    }
    auto logger_thread = Thread::Create(
        [&log_receiver]() { throw log_receiver->ReceiveForever().error(); });
    if (!HandleExpected(logger_thread)) {
        return 1;
    }

    auto logger = Logger::Create("main");
    if (!HandleExpected(logger)) {
        return 1;
    }

    logger->Debug("test");
    logger->Info("test");
    logger->Warning("test");
    logger->Error("test");

    // auto cancelled = logger_thread->Cancel();
    // if (!HandleExpected(cancelled)) {
    //     return 1;
    // }
    auto joined = logger_thread->Join();
    if (!HandleExpected(joined)) {
        return 1;
    }

    return 0;
}
