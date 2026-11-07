#include "../../internal/common/lockfreequeue/lockfreequeue.hpp"
#include <iostream>
#include <thread>

int main(int argc, char** argv) {
    LockFreeQueue<int> q(5);

   std::jthread producer([&]() {
        for(int i = 0; i < 20; ++i) {
            while(!q.enqueue(i)) {}
            std::cout << "Produced: " << i << std::endl;
        }
    });

    std::jthread consumer([&]() {
        for(int i = 0; i < 20; ++i) {
            int value;
            while(!q.dequeue(value)) {}
            std::cout << "Consumed: " << value << std::endl;
        }
    });

    return 0;
}