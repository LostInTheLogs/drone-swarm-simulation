#include <experimental/scope>
#include <iostream>

#include "systemv_ipc.h"

auto main(int /*argc*/, char* /*argv*/[]) -> int {
    auto msg_queue = IpcMessageQueue::Create(1234, 0666);
    if (!msg_queue) {
        std::cout << msg_queue.error().what() << '\n';
    }
    std::experimental::scope_exit guard{[&msg_queue] {
        auto removed = msg_queue->Remove();
        if (!removed) {
            std::cout << removed.error().what() << '\n';
        }
    }};

    auto sent = msg_queue->Send(15, 1);
    if (!sent) {
        std::cout << sent.error().what() << '\n';
    }

    auto received = msg_queue->Receive<int>(1);
    if (!received) {
        std::cout << received.error().what();
    }

    std::cout << received.value();
    return 0;
}
