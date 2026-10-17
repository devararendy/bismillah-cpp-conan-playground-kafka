#pragma once

#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

template <typename T>
class Channel {
private:
    std::queue<T> queue;
    std::mutex mtx;
    std::condition_variable cv;
    bool closed = false;

public:
    // Go: ch <- value (Send)
    void send(T value) {
        std::unique_lock<std::mutex> lock(mtx);
        if (closed) return;
        queue.push(std::move(value));
        cv.notify_one(); // Wake up one waiting consumer thread
    }

    // Go: value, ok := <-ch (Receive)
    std::optional<T> receive() {
        std::unique_lock<std::mutex> lock(mtx);
        cv.wait(lock, [this]() { return !queue.empty() || closed; });
        
        if (queue.empty() && closed) {
            return std::nullopt; // Channel is empty and closed (Go's ok == false)
        }
        
        T value = std::move(queue.front());
        queue.pop();
        return value;
    }

    void close() {
        std::unique_lock<std::mutex> lock(mtx);
        closed = true;
        cv.notify_all(); // Wake up all waiting threads to drain
    }
};
