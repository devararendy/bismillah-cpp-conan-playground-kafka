#include "../../internal/common/channel/channel.hpp"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <future>
#include <iostream>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <vector>

namespace concurrent_example {

enum class Side { Buy, Sell };

struct OrderDetail {
    std::string account_id;
    std::string order_id;
    int qty_in_lots;
    Side side;
};

using OrderDetails = std::vector<OrderDetail>;
using OrderPerAccounts = std::unordered_map<std::string, OrderDetails>;

class OrderProcessor {
public:
    [[nodiscard]] static std::pair<int, std::optional<std::string>> process(
        const OrderDetails& orders) {
        int lot_balance = 0;
        for (const auto& order : orders) {
            if (order.side == Side::Buy) {
                lot_balance += order.qty_in_lots;
            } else if (order.side == Side::Sell) {
                lot_balance -= order.qty_in_lots;
            } else {
                return {0, "invalid order side"};
            }
        }
        return {lot_balance, std::nullopt};
    }
};

struct Result {
    int worker_id;
    int lot_balance;
    std::optional<std::string> error;
};

class ThreadPool {
public:
    explicit ThreadPool(std::size_t worker_count) {
        worker_count = worker_count == 0 ? 1 : worker_count;
        workers_.reserve(worker_count);
        for (std::size_t i = 0; i < worker_count; ++i) {
            workers_.emplace_back([this] { workerLoop(); });
        }
    }

    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;

    ~ThreadPool() {
        shutdown();
    }

    template <typename Function>
    [[nodiscard]] auto submit(Function&& function)
        -> std::future<std::invoke_result_t<Function>> {
        using ReturnType = std::invoke_result_t<Function>;
        auto task = std::make_shared<std::packaged_task<ReturnType()>>(
            std::forward<Function>(function));
        std::future<ReturnType> result = task->get_future();

        {
            std::scoped_lock lock(mutex_);
            if (stopped_) {
                throw std::runtime_error("cannot submit to a stopped thread pool");
            }
            tasks_.emplace([task] { (*task)(); });
        }
        condition_.notify_one();
        return result;
    }

private:
    void workerLoop() {
        for (;;) {
            std::packaged_task<void()> task([] {});
            {
                std::unique_lock lock(mutex_);
                condition_.wait(lock, [this] { return stopped_ || !tasks_.empty(); });
                if (stopped_ && tasks_.empty()) {
                    return;
                }
                task = std::move(tasks_.front());
                tasks_.pop();
            }
            task();
        }
    }

    void shutdown() {
        {
            std::scoped_lock lock(mutex_);
            if (stopped_) {
                return;
            }
            stopped_ = true;
        }
        condition_.notify_all();
        for (auto& worker : workers_) {
            if (worker.joinable()) {
                worker.join();
            }
        }
    }

    std::vector<std::thread> workers_;
    std::queue<std::packaged_task<void()>> tasks_;
    std::mutex mutex_;
    std::condition_variable condition_;
    bool stopped_ = false;
};

[[nodiscard]] OrderPerAccounts buildDummyOrders() {
    return {
        {"account1", {
            {"account1", "order1", 10, Side::Buy},
            {"account1", "order2", 5, Side::Sell},
        }},
        {"account2", {
            {"account2", "order3", 20, Side::Buy},
            {"account2", "order4", 15, Side::Sell},
        }},
    };
}

void calcUsingNative(const OrderPerAccounts& orders_per_account) {
    constexpr std::size_t worker_count = 10;
    constexpr std::size_t buffer_size = 20;

    Channel<OrderDetails> jobs;
    Channel<Result> results;
    std::vector<std::jthread> workers;
    workers.reserve(worker_count);

    std::atomic<bool> cancelled = false;
    for (std::size_t worker_id = 0; worker_id < worker_count; ++worker_id) {
        workers.emplace_back([worker_id, &jobs, &results, &cancelled] {
            while (!cancelled.load(std::memory_order_relaxed)) {
                auto orders = jobs.receive();
                if (!orders.has_value()) {
                    break;
                }

                const auto [lot_balance, error] = OrderProcessor::process(*orders);
                results.send(Result{
                    static_cast<int>(worker_id),
                    lot_balance,
                    error,
                });
            }
        });
    }

    std::jthread producer([&orders_per_account, &jobs] {
        for (const auto& [account, orders] : orders_per_account) {
            static_cast<void>(account);
            jobs.send(orders);
        }
        jobs.close();
    });

    results.close();

    int required_broker_limit = 0;
    while (auto result = results.receive()) {
        if (result->error) {
            std::cerr << "Worker " << result->worker_id
                      << " error: " << *result->error << '\n';
            continue;
        }
        required_broker_limit += result->lot_balance;
    }

    std::cout << "calcUsingNative - required_broker_limit: "
              << required_broker_limit << '\n';
}

void calcUsingPool(const OrderPerAccounts& orders_per_account) {
    constexpr std::size_t max_concurrency = 10;
    ThreadPool pool(max_concurrency);
    std::vector<std::future<Result>> submitted;
    submitted.reserve(orders_per_account.size());

    int worker_id = 0;
    for (const auto& [account, orders] : orders_per_account) {
        submitted.emplace_back(pool.submit([worker_id, &account, &orders] {
            const auto [lot_balance, error] = OrderProcessor::process(orders);
            if (error) {
                std::cerr << "Account " << account << " worker " << worker_id
                          << " error: " << *error << '\n';
            }
            return Result{worker_id, lot_balance, error};
        }));
        ++worker_id;
    }

    int required_broker_limit = 0;
    for (auto& task : submitted) {
        const Result result = task.get();
        if (!result.error) {
            required_broker_limit += result.lot_balance;
        }
    }

    std::cout << "calcUsingPool - required_broker_limit: "
              << required_broker_limit << '\n';
}

void calcUsingAsync(const OrderPerAccounts& orders_per_account) {
    std::vector<std::future<Result>> submitted;
    submitted.reserve(orders_per_account.size());

    int worker_id = 0;
    for (const auto& [account, orders] : orders_per_account) {
        submitted.emplace_back(std::async(
            std::launch::async,
            [worker_id, &account, &orders] {
                const auto [lot_balance, error] = OrderProcessor::process(orders);
                if (error) {
                    std::cerr << "Account " << account << " worker " << worker_id
                              << " error: " << *error << '\n';
                }
                return Result{worker_id, lot_balance, error};
            }));
        ++worker_id;
    }

    int required_broker_limit = 0;
    for (auto& task : submitted) {
        const Result result = task.get();
        if (!result.error) {
            required_broker_limit += result.lot_balance;
        }
    }

    std::cout << "calcUsingAsync - required_broker_limit: "
              << required_broker_limit << '\n';
}

} // namespace concurrent_example

int main() {
    using namespace concurrent_example;

    const auto orders = buildDummyOrders();

    std::cout << "Using native worker channels\n";
    calcUsingNative(orders);

    std::cout << "Using a fixed C++ thread pool\n";
    calcUsingPool(orders);

    std::cout << "Using std::async with futures\n";
    calcUsingAsync(orders);

    return 0;
}