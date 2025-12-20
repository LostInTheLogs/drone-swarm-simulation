#include <unistd.h>

#include <csignal>
#include <experimental/scope>

#include "logger.h"
#include "process.h"

namespace {
auto Err(const auto& val) -> auto& {
    if (!val) {
        throw std::move(val.error());
    }
    return val.value();
}
}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    try {
        auto logger_process = Err(Process::CreateReady({"./logger"}));

        // const auto& logger = Err(Logger::Create("main"));

        auto drone_process = Err(Process::Create({"./drone"}));

        Err(drone_process.Wait());
        sleep(1);
        Err(logger_process.TermWait());
    } catch (std::exception& e) {
        LogPrinter::PrintError("main", e.what());
        return 1;
    }
    return 0;
}
