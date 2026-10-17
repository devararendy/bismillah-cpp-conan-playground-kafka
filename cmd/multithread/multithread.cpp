#include <iostream>
#include <thread>
#include <chrono>
#include "../../internal/common/channel/channel.hpp"

// Producer Task (Runs on Thread A)
void producer(Channel<int>& ch) {
    for (int i = 1; i <= 3; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); // Simulate work
        std::cout << "[Producer] Sending: " << i << "\n";
        ch.send(i); // Push to channel
    }
    std::cout << "[Producer] Closing channel\n";
    ch.close();
}

// Consumer Task (Runs on Thread B)
void consumer(Channel<int>& ch) {
    while (true) {
        auto msg = ch.receive();
        if (!msg.has_value()) {
            std::cout << "[Consumer] Channel closed. Exiting.\n";
            break;
        }
        std::cout << "[Consumer] Received: " << msg.value() << "\n";
    }
}

int main() {
    Channel<int> data_channel;

    // Launch threads (the exact structural equivalent of writing 'go producer(ch)')
    std::thread producer_thread(producer, std::ref(data_channel));
    std::thread consumer_thread(consumer, std::ref(data_channel));

    // std::thread must be joined before it is destroyed.
    producer_thread.join();
    consumer_thread.join();

    return 0;
}
