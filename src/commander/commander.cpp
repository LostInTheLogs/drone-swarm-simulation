#include <sys/wait.h>

#include <cstdio>
#include <format>
#include <iostream>

#include "globals.h"
#include "ipc/semaphore_set.h"
#include "logger.h"
#include "mutex.h"
#include "process.h"

namespace {
void Err(auto&& val) {
    if (!val) {
        throw std::forward<decltype(val)>(val).error();
    }
}
}  // namespace

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    using namespace std::chrono_literals;

    try {
        auto shm_params = ShmParameters::Get(ShmKey::PARAMS);

        const auto drones_arr_size =
            ShmDrones::value_type::CalcSize(shm_params->init_drone_count);
        auto sems = SemaphoreSet<SemIds>::Get(SemSetKey::MAIN);

        auto drones = ShmDrones::Get(ShmKey::DRONES, drones_arr_size);
        auto mut = RWMutex::Get<RWMUT_SEMS(DRONES)>(sems);
        const auto operator_proc = Process(shm_params->operator_pid);

        shm_params.Detach();

        auto logger = Logger::Create("operator");

        const auto list = [&]() {
            std::cout << "Drones: ";
            for (size_t i = 0; i < drones->Size(); i++) {
                auto drone = (*drones)[i];
                std::cout << std::format("[{}]: {}, ", i, drone);
            }
            std::cout << '\n';
        };

        const auto send_suicide = [&]() {
            size_t index{};
            std::cout << "choose a drone (index): ";
            std::cin >> index;
            if (index >= drones->Size()) {
                std::cout << "Drone not found!\n";
                return;
            }
            Process((*drones)[index]).Signal(SIGUSR1);
            logger.Info("Sent suicide order");
            std::cout << "Order sent!\n";
        };

        char choice{};
        while (choice != 'q') {
            std::cout << "\n"
                      << "Menu:\n"
                      << "q: quit\n"
                      << "l: list drone PIDs\n"
                      << "s: send drone on a suicide mission\n"
                      << "i: increase max drone count\n"
                      << "d: decrease max drone count\n"
                      << "> ";
            std::cin >> choice;
            if (CurrentProcess::TerminateReceived() || std::cin.eof()) {
                return 0;
            }

            switch (choice) {
                case 'q':
                    break;
                case 'l':
                    Err(mut.LockRead());
                    list();
                    mut.UnlockRead();
                    break;
                case 's':
                    Err(mut.LockRead());
                    list();
                    send_suicide();
                    mut.UnlockRead();
                    break;
                case 'i':
                    operator_proc.Signal(SIGUSR1);
                    std::cout << "Order sent!\n";
                    break;
                case 'd':
                    operator_proc.Signal(SIGUSR2);
                    std::cout << "Order sent!\n";
                    break;
                default:
                    std::cout << "This options does not exist!\n";
                    break;
            }
        }

    } catch (std::exception& e) {
        LogPrinter::PrintError("commander", e.what());
        LogPrinter::PrintError("commander", "TIP: Is the simulation running?");
        return 1;
    }

    return 0;
}
