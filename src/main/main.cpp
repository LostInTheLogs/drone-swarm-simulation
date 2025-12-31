#include <unistd.h>

#include <csignal>

#include "globals.h"
#include "ipc/ipc.h"
#include "ipc/semaphore_set.h"
#include "ipc/shared_memory.h"
#include "logger.h"
#include "process.h"
#include "queue.h"
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

        const auto queue_size =
            Queue<pid_t>::CalcSize(shm_params->init_drone_count);
        auto in_queue =
            ShmProcQueue::Create(ShmKey::IN_QUEUE, 0666, queue_size);
        auto out_queue =
            ShmProcQueue::Create(ShmKey::OUT_QUEUE, 0666, queue_size);

        const auto drones_arr_size =
            ShmDrones::value_type::CalcSize(shm_params->init_drone_count);
        auto drones_arr =
            ShmDrones::Create(ShmKey::DRONES, 0666, drones_arr_size);

        auto base = ShmBaseData::Create(ShmKey::BASE_DATA, 0666);

        auto sems = SemaphoreSet<SemIds>::Create(SemSetKey::MAIN, 0666);

        shm_params.Detach();

        auto logger_process = Process::CreateReady({"./logger"});

        auto operator_proc = Process::Create({"./operator"});
        operator_proc.Wait();

        auto slept = Thread::SleepFor(1000ms);
        logger_process.TermWait();
    } catch (std::exception& e) {
        LogPrinter::PrintError("main", e.what());
        return 1;
    }
    return 0;
}
