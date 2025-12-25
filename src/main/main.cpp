#include <unistd.h>

#include <csignal>
#include <iostream>

#include "globals.h"
#include "ipc/semaphore_set.h"
#include "ipc/shared_memory.h"
#include "logger.h"
#include "process.h"
#include "shm_queue.h"
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
        // auto params = ShmParameters::Create(ShmKey::PARAMS, 0666);
        // params->max_drones = 10;
        //
        // const auto queue_size =
        // SmhQueue<pid_t>::CalcSize(params->max_drones); auto in_queue =
        //     ShmProcQueue::Create(ShmKey::IN_QUEUE, 0666, queue_size);
        // in_queue.Detach();
        // auto out_queue =
        //     ShmProcQueue::Create(ShmKey::OUT_QUEUE, 0666, queue_size);
        // out_queue.Detach();
        //
        // params.Detach();

        // auto sems = Err(
        //     SemaphoreSet<TestSem>::Create(SemaphoreSetKey::MAIN, {1}, 0666));

        auto logger_process = Process::CreateReady({"./logger"});

        // const auto& logger = Err(Logger::Create("main"));

        auto drone_process = Process::Create({"./drone"});

        drone_process.Wait();
        auto slept = Thread::SleepFor(1s);
        logger_process.TermWait();
    } catch (std::exception& e) {
        LogPrinter::PrintError("main", e.what());
        return 1;
    }
    return 0;
}
