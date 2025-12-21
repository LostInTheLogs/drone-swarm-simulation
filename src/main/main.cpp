#include <unistd.h>

#include <csignal>

#include "logger.h"
#include "process.h"
#include "thread.h"

namespace {
auto Err(auto&& val) -> decltype(auto) {
    if (!val) {
        throw std::forward<decltype(val)>(val).error();
    }
    return std::forward<decltype(val)>(val).value();
}
}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    using namespace std::chrono_literals;
    try {
        auto logger_process = Err(Process::CreateReady({"./logger"}));

        // const auto& logger = Err(Logger::Create("main"));

        auto drone_process = Err(Process::Create({"./drone"}));

        Err(drone_process.Wait());
        auto slept = Thread::SleepFor(1s);
        Err(logger_process.TermWait());
    } catch (std::exception& e) {
        LogPrinter::PrintError("main", e.what());
        return 1;
    }
    return 0;
}
