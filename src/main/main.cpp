#include <unistd.h>

#include <csignal>

#include "globals.h"
#include "ipc/ipc.h"
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
        auto shm_params = ShmParameters::Create(ShmKey::PARAMS, 0666);
        shm_params->max_drones = 10;

        const auto queue_size =
            SmhQueue<pid_t>::CalcSize(shm_params->max_drones);
        auto shm_in_queue =
            ShmProcQueue::Create(ShmKey::QUEUE1, 0666, queue_size);
        shm_in_queue.Detach();
        auto smh_out_queue =
            ShmProcQueue::Create(ShmKey::QUEUE2, 0666, queue_size);
        smh_out_queue.Detach();

        shm_params.Detach();

        auto sems = SemaphoreSet<SemIds>::Create(SemSetKey::MAIN, 0666);

        auto logger_process = Process::CreateReady({"./logger"});

        // const auto& logger = Err(Logger::Create("main"));

        auto drone_process = Process::Create({"./drone"});

        drone_process.Wait();
        auto slept = Thread::SleepFor(100ms);
        logger_process.TermWait();
    } catch (std::exception& e) {
        LogPrinter::PrintError("main", e.what());
        return 1;
    }
    return 0;
}
