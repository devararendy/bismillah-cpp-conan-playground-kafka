# C++ Concurrent Example

This document explains [concurrent.cpp](./concurrent.cpp) line by line. The program demonstrates four ways to process account orders concurrently:

1. native worker threads with the existing `Channel` class;
2. a custom fixed-size thread pool;
3. `std::async` with `std::future`; and
4. C++ coroutines that await per-account work scheduled by `std::async`.

For the sample data, each method prints a required broker limit of `10` lots.

## How to build and run

```bash
cmake --build build/release --target concurent -j4
./build/release/concurent
```

The CMake target is named `concurent` in the repository's CMake configuration.

## Includes

```cpp
#include "../../internal/common/channel/channel.hpp"
```

Includes the local channel implementation used by the native worker example. The first approach uses `Channel<T>`, while the other two approaches do not depend on it.

```cpp
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <coroutine>
#include <functional>
#include <future>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <queue>
#include <stdexcept>
#include <string>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>
```

- `<atomic>` provides `std::atomic`, which performs thread-safe operations with a defined memory model.
- `<chrono>` provides durations and `std::chrono::seconds` for polling whether a coroutine awaitable is ready.
- `<condition_variable>` lets pool workers wait until work is available or shutdown begins.
- `<coroutine>` provides coroutine handles, promise types, and related coroutine support types.
- `<functional>` provides the callable wrapper types used by higher-level concurrency tools.
- `<future>` provides `std::future`, `std::async`, and `std::packaged_task`.
- `<iostream>` provides `std::cout` and `std::cerr`.
- `<memory>` provides shared ownership through `std::shared_ptr` and `std::make_shared`.
- `<mutex>` provides mutual exclusion types.
- `<optional>` represents a value that may or may not exist. It is used for process errors and channel receive results.
- `<queue>` stores pending tasks in the custom thread pool and queued values in the channel implementation.
- `<stdexcept>` provides exceptions such as `std::runtime_error`.
- `<string>` provides account, order, and error text.
- `<thread>` provides `std::thread`.
- `<type_traits>` provides compile-time type utilities such as `std::invoke_result_t`.
- `<unordered_map>` stores orders grouped by account.
- `<utility>` provides `std::forward`, `std::exchange`, and `std::move`.
- `<vector>` stores orders, futures, and worker threads.

## Namespace

```cpp
namespace concurrent_example {
```

Creates the `concurrent_example` namespace, preventing names such as `Result`, `Side`, and `OrderDetail` from polluting the global namespace.

```cpp
} // namespace concurrent_example
```

Closes the namespace. This comment documents the matching namespace declaration.

## Order types

```cpp
enum class Side { Buy, Sell };
```

Defines a scoped enumeration. `enum class` prevents implicit conversion to integers and requires names such as `Side::Buy`, making invalid combinations harder to create.

```cpp
struct OrderDetail {
    std::string account_id;
    std::string order_id;
    int qty_in_lots;
    Side side;
};
```

Defines an individual order. `struct` uses public members by default. It stores the account, order identifier, quantity, and side.

```cpp
using OrderDetails = std::vector<OrderDetail>;
using OrderPerAccounts = std::unordered_map<std::string, OrderDetails>;
```

- `OrderDetails` is a list of orders belonging to one account.
- `OrderPerAccounts` maps an account name to its order list.
- `using` creates readable type aliases instead of repeating the underlying container syntax.

## Order processor

```cpp
class OrderProcessor {
public:
```

Defines a stateless processor. `public:` means the members declared after it can be accessed from outside the class. The class is stateless because processing does not depend on stored object state.

```cpp
[[nodiscard]] static std::pair<int, std::optional<std::string>> process(
    const OrderDetails& orders) {
```

- `static` means callers can invoke `OrderProcessor::process(...)` without constructing an object.
- `const OrderDetails& orders` accepts the order list by const reference. This avoids copying all orders and still prevents modification of the input list.
- `std::pair<int, std::optional<std::string>>` returns both a lot balance and an optional error.
- `[[nodiscard]]` tells the compiler to warn when the returned value is discarded. This is important because ignoring the result would silently lose either the calculated lot balance or the processing error.

`[[nodiscard]]` is a compiler attribute written in C++17 and works especially well for functions that return resources or results such as futures, locks, and calculated values.

```cpp
int lot_balance = 0;
```

Initializes the running result to zero. `BUY` orders increase it, while `SELL` orders decrease it.

```cpp
for (const auto& order : orders) {
```

Iterates over every order using a range-based `for`. `const auto&` deduces the element type and stores a const reference, avoiding a copy without permitting modification of the original order.

```cpp
if (order.side == Side::Buy) {
    lot_balance += order.qty_in_lots;
}
```

For a `BUY`, adds the order quantity to the balance. `+=` is equivalent to `lot_balance = lot_balance + order.qty_in_lots`.

```cpp
} else if (order.side == Side::Sell) {
    lot_balance -= order.qty_in_lots;
}
```

For a `SELL`, subtracts the order quantity. In C++, the closing structure follows the opening `if`; this `else if` handles only the remaining valid side.

```cpp
} else {
    return {0, "invalid order side"};
}
```

Although `Side` is currently limited to two values, this branch preserves defensive validation. It returns zero balance and an error string. Initializing the pair with `{0, "invalid order side"}` uses C++ aggregate construction.

```cpp
return {lot_balance, std::nullopt};
```

Returns the successful result. `std::nullopt` means the optional error has no value, so no processing error occurred.

The function effectively returns a pair of:

```text
(balance, optional_error)
```

## Result

```cpp
struct Result {
    int worker_id;
    int lot_balance;
    std::optional<std::string> error;
};
```

Defines one worker outcome. It identifies the worker, stores its lot balance, and optionally stores an error.

A `std::optional<std::string>` has three useful states:

- an error string exists;
- the optional is empty; or
- `has_value()`/`operator bool` can be tested before dereferencing.

## Custom thread pool

```cpp
class ThreadPool {
public:
```

Defines the pool and exposes constructor, destructor, copy semantics, and task submission publicly.

### Constructor

```cpp
explicit ThreadPool(std::size_t worker_count) {
```

The constructor creates the pool. `explicit` prevents implicit conversion from an integer to a `ThreadPool`, so expressions such as assigning `ThreadPool pool = 10;` will not compile accidentally.

```cpp
worker_count = worker_count == 0 ? 1 : worker_count;
```

Guarantees at least one worker. If zero is requested, the ternary expression evaluates to `1`; otherwise, it keeps the requested count.

```cpp
workers_.reserve(worker_count);
```

Preallocates storage for worker objects. This does not create the threads; it prevents unnecessary vector reallocations during `emplace_back`.

```cpp
for (std::size_t i = 0; i < worker_count; ++i) {
    workers_.emplace_back([this] { workerLoop(); });
}
```

Creates one thread per worker.

- `std::size_t` is an unsigned integer type intended for sizes and counts.
- `emplace_back` constructs the new `std::thread` directly inside the vector.
- `[this]` captures the current pool object, allowing the thread entry function to call `workerLoop()`.
- Each thread begins immediately running the lambda, which calls the member function.

### Copy prevention

```cpp
ThreadPool(const ThreadPool&) = delete;
ThreadPool& operator=(const ThreadPool&) = delete;
```

Deleting copy operations prevents copying an object that owns threads, mutexes, queued tasks, and shutdown state. Copies would cause ambiguous ownership and potentially make both copies try to join or destroy the same resources.

### Destructor

```cpp
~ThreadPool() {
    shutdown();
}
```

Automatically shuts down the pool when it is destroyed. This ensures worker threads are signaled and joined instead of being left running after pool destruction.

### Task submission

```cpp
template <typename Function>
[[nodiscard]] auto submit(Function&& function)
    -> std::future<std::invoke_result_t<Function>> {
```

This function is a template and can submit any callable with no required arguments.

- `Function&&` is a forwarding reference, not always an rvalue reference. It can accept both lvalues and rvalues while preserving value category.
- `std::invoke_result_t<Function>` computes the callable's return type at compile time using type traits.
- `std::future<ReturnType>` lets the caller retrieve that result later.
- The trailing return type after `->` explicitly documents the future type.
- `[[nodiscard]]` requires callers not to ignore the returned `std::future`; ignoring it would lose the task's result and exception information.

This distinction matters because many errors or successful values are stored by the future after `submit()` returns.

```cpp
using ReturnType = std::invoke_result_t<Function>;
```

Creates a local type alias so the packaged task and future declarations remain readable.

```cpp
auto task = std::make_shared<std::packaged_task<ReturnType()>>(
    std::forward<Function>(function));
```

Creates a shared pointer to a package that can be called with no arguments and return `ReturnType`.

- `std::packaged_task` wraps the callable and stores its result in an associated future.
- `ReturnType()` describes a callable taking no parameters and returning `ReturnType`.
- `std::make_shared` allocates and owns the task safely.
- `std::forward` forwards the callable as an lvalue or rvalue according to what the caller supplied.
- A shared pointer is used because the task is moved into the queue and executed by a worker thread that may outlive the submit function's local variables.

```cpp
std::future<ReturnType> result = task->get_future();
```

Obtains the future before the task executes. The packaged task owns the shared state where its eventual result or exception will be stored.

```cpp
{
    std::scoped_lock lock(mutex_);
```

Begins a critical section. `std::scoped_lock` is an RAII mutex lock introduced in C++17; its constructor locks the mutex and its destructor unlocks it, including during exceptions.

RAII stands for Resource Acquisition Is Initialization: the resource lifetime follows object lifetime, so cleanup is automatic.

```cpp
if (stopped_) {
    throw std::runtime_error("cannot submit to a stopped thread pool");
}
```

Rejects new work after shutdown begins. The exception is catchable and prevents a future from being returned for work that was not queued.

```cpp
tasks_.emplace([task] { (*task)(); });
```

Adds a no-argument packaged task to the task queue under the mutex. The lambda captures the shared `task`, invokes it, and performs no explicit return because `packaged_task` already stores the underlying callable's result.

```cpp
}
condition_.notify_one();
return result;
```

The lock ends after the queue insertion. `notify_one()` wakes a waiting worker. The associated `std::future` is then returned.

### Worker loop

```cpp
void workerLoop() {
    for (;;) {
```

`workerLoop()` runs continuously until shutdown and the queue are both empty. `for (;;)` is an infinite loop; it has no condition or increment expression and is equivalent to a loop that continues until an explicit return.

```cpp
std::packaged_task<void()> task([] {});
```

Creates a placeholder task before acquiring real work. It is assigned a new task from the queue inside the loop. `[] {}` is an empty lambda taking no arguments and returning nothing. The task is of type `std::packaged_task<void()>` because the pool stores all tasks through a uniform function signature.

```cpp
{
    std::unique_lock lock(mutex_);
```

Creates a lock that can be released while waiting. `std::unique_lock` is required by `condition_variable::wait` because it temporarily unlocks and then re-locks the mutex.

```cpp
condition_.wait(lock, [this] {
    return stopped_ || !tasks_.empty();
});
```

Waits while neither shutdown has occurred nor queued work exists. The condition-variable predicate prevents lost wakeups and spurious wakeups by checking state again before returning from the wait.

The worker releases `mutex_` while sleeping, allowing another thread to submit work or initiate shutdown.

```cpp
if (stopped_ && tasks_.empty()) {
    return;
}
```

When shutdown is active and no work remains, the worker exits its loop. Work already queued before shutdown is still processed because this condition requires the queue to be empty.

```cpp
task = std::move(tasks_.front());
tasks_.pop();
```

Moves the queued task out of the queue instead of copying it. `packaged_task` is move-only, so moving is required. `pop()` removes the queue element afterward.

```cpp
}
task();
```

The lock scope ends, so worker threads do not hold the mutex while executing user work. Calling `task()` executes the queued callable outside the critical section and stores its return value or exception in its future.

### Shutdown

```cpp
void shutdown() {
    {
        std::scoped_lock lock(mutex_);
        if (stopped_) {
            return;
        }
        stopped_ = true;
    }
```

Marks the pool as stopped while holding the mutex. It also prevents repeated shutdown calls. If another destructor or caller already shut it down, this function returns without joining twice.

```cpp
condition_.notify_all();
```

Wakes every worker so each can observe the stopped state. Workers with queued tasks can finish them; workers with no work exit through the shutdown branch.

```cpp
for (auto& worker : workers_) {
    if (worker.joinable()) {
        worker.join();
    }
}
```

Waits for every worker to finish. Range iteration obtains references so the actual worker objects are not copied.

`joinable()` checks whether joining is valid. `join()` blocks until that worker exits, making sure no worker thread is destroyed while still running.

### Pool state

```cpp
std::vector<std::thread> workers_;
std::queue<std::packaged_task<void()>> tasks_;
std::mutex mutex_;
std::condition_variable condition_;
bool stopped_ = false;
```

- `workers_` owns the worker threads.
- `tasks_` owns the queued callable objects.
- `mutex_` protects `tasks_` and `stopped_`.
- `condition_` coordinates submissions and waiting workers.
- `stopped_` records shutdown state.

## Sample orders

```cpp
[[nodiscard]] OrderPerAccounts buildDummyOrders() {
```

`buildDummyOrders()` returns ownership of a complete sample map. `[[nodiscard]]` warns if the caller ignores the map, because doing so would discard the generated test data.

The function returns data for two accounts.

```cpp
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
```

This is nested initializer-list construction:

- outer entries map account names to order vectors;
- inner entries construct `OrderDetail` objects from strings, quantity, and side; and
- no explicit type names are required because the compiler uses the return type and container element types.

Expected account balances are:

- `account1`: `10 - 5 = 5`;
- `account2`: `20 - 15 = 5`;
- total: `10` lots.

## Native channel method

```cpp
void calcUsingNative(const OrderPerAccounts& orders_per_account) {
```

Processes all accounts with explicitly created worker threads. The input map is const and passed by reference.

```cpp
constexpr std::size_t worker_count = 10;
constexpr std::size_t buffer_size = 20;
```

`constexpr` makes these values compile-time constants. The native implementation creates ten workers. `buffer_size` is currently declared but not used by this source file; the underlying channel controls its own queue capacity.

```cpp
Channel<OrderDetails> jobs;
Channel<Result> results;
```

Creates one channel for jobs and one for worker results. The channel is a queue with mutex and condition-variable synchronization implemented by the local library.

```cpp
std::vector<std::thread> workers;
workers.reserve(worker_count);
```

Creates a vector capable of storing ten worker thread objects and reserves storage in advance.

```cpp
std::atomic<bool> cancelled = false;
```

Creates a thread-safe cancellation flag. An atomic variable prevents a single unsynchronized read/write operation from becoming a data race. In this program it is checked but never changed; it can be wired into cancellation handling if needed.

```cpp
for (std::size_t worker_id = 0;
     worker_id < worker_count;
     ++worker_id) {
```

Gives every worker a unique identifier. `++worker_id` increments after each worker thread is launched.

### Worker lambda

```cpp
workers.emplace_back([worker_id, &jobs, &results, &cancelled] {
```

Creates a thread with a lambda. The lambda captures:

- `worker_id` by value, giving each worker its own ID;
- `jobs` and `results` by reference, sharing the channels between threads; and
- `cancelled` by reference, allowing access to the shared atomic state.

References remain valid because the channels and atomic exist until `calcUsingNative()` completes and all joins finish.

```cpp
while (!cancelled.load(std::memory_order_relaxed)) {
```

Continues the worker until cancellation becomes true. `load()` atomically reads the flag. `std::memory_order_relaxed` guarantees atomicity but provides no synchronization ordering for other non-atomic values. It is sufficient here because only the flag itself is important.

A stricter memory order would be appropriate if canceling this flag published results that unrelated threads must also observe in a specific order.

```cpp
auto orders = jobs.receive();
```

Receives one job from the channel. The return type is `std::optional<OrderDetails>`: it contains a job when available and becomes empty after the channel closes and drains.

```cpp
if (!orders.has_value()) {
    break;
}
```

Exits the loop when the jobs channel is closed and empty. `has_value()` is the explicit optional query.

```cpp
const auto [lot_balance, error] = OrderProcessor::process(*orders);
```

Calls the processor and uses C++ structured binding to split the returned pair into named variables.

- `auto` deduces the types from `process()`;
- `[lot_balance, error]` creates each bound variable with the corresponding pair member;
- `const` makes the structured-binding names const;
- `*orders` dereferences the optional after confirming that it has a value.

```cpp
results.send(Result{
    static_cast<int>(worker_id),
    lot_balance,
    error,
});
```

Constructs and sends a `Result` directly through the channel. `static_cast<int>(worker_id)` explicitly converts the worker's `std::size_t` identifier to the result's `int` field.

The conversion is explicit because integer types of different signedness can be surprising if converted implicitly.

### Producer thread

```cpp
std::thread producer([&orders_per_account, &jobs] {
```

Creates a separate producer thread. Both arguments are captured by reference because no copy is needed and the producer cannot outlive the function without being joined.

```cpp
for (const auto& [account, orders] : orders_per_account) {
```

Iterates over the account map. Structured binding splits each map pair into an account key and an order vector.

```cpp
static_cast<void>(account);
```

Silences the unused-variable warning for `account`. The native implementation sends only the order vector; account data is not needed inside the worker result.

This is also equivalent conceptually to explicitly discarding the key without declaring a second loop variable.

```cpp
jobs.send(orders);
```

Adds the account's orders to the jobs channel, waking a waiting worker.

```cpp
jobs.close();
```

Marks the jobs channel closed. The channel can still be emptied through `receive()`, but new sends should not be accepted.

```cpp
producer.join();
```

Blocks until all account jobs have been sent and the producer closes the jobs channel.

### Worker joining

```cpp
for (auto& worker : workers) {
    if (worker.joinable()) {
        worker.join();
    }
}
```

Waits for all workers. `joinable()` verifies that joining is legal, then `join()` waits for each worker to observe channel closure and exit.

```cpp
results.close();
```

Close the results channel only after all workers have sent their results and exited. This allows the receiving loop to stop when the queue is empty and closed.

```cpp
int required_broker_limit = 0;
```

Initializes the aggregate broker requirement.

```cpp
while (auto result = results.receive()) {
```

Keeps receiving until an empty optional is returned. This loop form combines receive and optional validity testing.

```cpp
if (result->error) {
```

Checks the optional error. The arrow operator accesses the `Result` through the optional value.

```cpp
std::cerr << "Worker " << result->worker_id
          << " error: " << *result->error << '\n';
```

Writes the worker ID and error to the standard error stream. `*result->error` dereferences the `optional<string>` to obtain the actual string.

The `<<` operator is overload-based stream insertion. `std::endl` could flush output, but `'\n'` is sufficient here and usually adds no unnecessary flush.

```cpp
continue;
```

Skips the balance addition for a failed result.

```cpp
required_broker_limit += result->lot_balance;
```

Adds successful worker balances to the total.

```cpp
std::cout << "calcUsingNative - required_broker_limit: "
          << required_broker_limit << '\n';
```

Prints the final native-method result to standard output.

## Custom thread pool method

```cpp
void calcUsingPool(const OrderPerAccounts& orders_per_account) {
```

Runs one pool task per account and retrieves each result from a future.

```cpp
constexpr std::size_t max_concurrency = 10;
ThreadPool pool(max_concurrency);
```

Sets a maximum worker count of ten and constructs the custom pool. Because tasks queue when more work exists, concurrency is capped at the number of worker threads.

```cpp
std::vector<std::future<Result>> submitted;
submitted.reserve(orders_per_account.size());
```

Creates a future vector and reserves enough capacity for one result per account, avoiding repeated reallocations while submissions occur.

```cpp
int worker_id = 0;
```

Initializes a numeric task identifier.

```cpp
for (const auto& [account, orders] : orders_per_account) {
```

Creates a task for each account. Structured binding names the account key and corresponding orders.

```cpp
submitted.emplace_back(pool.submit([worker_id, &account, &orders] {
```

Submits a lambda to the pool and stores its future in `submitted`.

- `worker_id` is captured by value, so each submitted lambda retains a distinct ID.
- `account` and `orders` are references to the current loop map entries.

The lambda's lifetime ends only after pool execution; but the pool's destructor joins threads before the local input map goes out of scope, so references remain valid.

```cpp
const auto [lot_balance, error] = OrderProcessor::process(orders);
```

Processes one account and splits the pair result.

```cpp
if (error) {
    std::cerr << "Account " << account << " worker " << worker_id
              << " error: " << *error << '\n';
}
```

Reports a processing error. Because `std::optional` defines an explicit boolean conversion to indicate whether a value exists, `if (error)` checks whether an error string is present.

```cpp
return Result{worker_id, lot_balance, error};
```

Returns the worker result. The future stores this value and any exception thrown by the task.

```cpp
++worker_id;
```

Uses prefix increment to assign the next task a distinct worker ID. Prefix increment generally avoids returning or copying the old temporary value and communicates “increment immediately.”

```cpp
int required_broker_limit = 0;
for (auto& task : submitted) {
    const Result result = task.get();
```

Initializes the total and iterates through submitted futures. `get()` blocks until that task is complete, then returns the stored `Result`.

Calling `get()` on a future also retrieves and rethrows a stored exception. A future is single-use for data retrieval.

```cpp
if (!result.error) {
    required_broker_limit += result.lot_balance;
}
```

Adds the balance only when the optional has no error.

```cpp
std::cout << "calcUsingPool - required_broker_limit: "
          << required_broker_limit << '\n';
```

Prints the pool result.

## `std::async` method

```cpp
void calcUsingAsync(const OrderPerAccounts& orders_per_account) {
```

Processes account tasks with the standard library's high-level async launcher.

```cpp
std::vector<std::future<Result>> submitted;
submitted.reserve(orders_per_account.size());
```

Stores one future per account and reserves capacity in advance.

```cpp
int worker_id = 0;
for (const auto& [account, orders] : orders_per_account) {
```

Assigns a task ID and processes each account entry.

```cpp
submitted.emplace_back(std::async(
    std::launch::async,
    [worker_id, &account, &orders] {
```

- `std::launch::async` explicitly requests execution in a new thread.
- It differs from `std::launch::deferred`, which would defer execution until `get()` or `wait()` is called on the associated future.
- Without an explicit launch policy, the standard library could choose implementation-dependent deferred or async behavior.
- The lambda returns a `Result` and is executed asynchronously.

The worker ID is captured by value. Account and order references remain valid through the `future::get()` loop at the end of the function.

```cpp
const auto [lot_balance, error] = OrderProcessor::process(orders);
```

Processes one account in the async task.

```cpp
if (error) {
    std::cerr << "Account " << account << " worker " << worker_id
              << " error: " << *error << '\n';
}
```

Reports any processing error from the async task.

```cpp
return Result{worker_id, lot_balance, error};
```

Returns the result for storage by the future. Exceptions would also be transferred through the future instead of being thrown directly into the caller thread during `std::async` launch.

```cpp
++worker_id;
```

Assigns the next async task a new ID.

```cpp
int required_broker_limit = 0;
for (auto& task : submitted) {
    const Result result = task.get();
    if (!result.error) {
        required_broker_limit += result.lot_balance;
    }
}
```

Retrieves each future in submission order. `get()` waits for completion and extracts the result. This loop is not collecting futures randomly; it waits in vector order and preserves deterministic reporting of the aggregate sum.

`std::async` may create tasks that run concurrently, but retrieving results through this loop does not necessarily preserve task completion order because futures are checked in the vector's submission order.

```cpp
std::cout << "calcUsingAsync - required_broker_limit: "
          << required_broker_limit << '\n';
```

Prints the final `std::async` result.

## C++ coroutine method

The coroutine implementation combines a custom coroutine task, an awaitable around `std::future`, and one asynchronous account operation per coroutine.

```cpp
template <typename TResult>
class AsyncAwaitable {
```

`AsyncAwaitable<TResult>` implements the protocol required by `co_await`. `TResult` is the type produced when the awaitable resumes.

```cpp
template <typename Function>
explicit AsyncAwaitable(Function&& function)
    : state_(std::make_shared<State>(
          std::async(std::launch::async, std::forward<Function>(function)))) {}
```

The constructor immediately starts the supplied callable with `std::async(std::launch::async)`.

### What does `Function&&` mean?

There are two uses of `&&` in C++, depending on whether the referenced type is deduced.

- In `Function&&` here, `Function` is a template parameter being deduced by the function call. This is a **forwarding reference** (also called an universal reference), not simply an rvalue reference.
- An ordinary rvalue reference, such as `std::string&&`, binds only to an rvalue. A forwarding reference can bind to either an lvalue or an rvalue.

For example:

```cpp
auto lambda = [] { return Result{}; };

AsyncAwaitable<Result> from_lvalue{lambda};       // lambda is an lvalue
AsyncAwaitable<Result> from_rvalue{[] { return Result{}; }}; // lambda is an rvalue
```

Both calls are valid. The forwarding reference preserves which kind of value was supplied:

```cpp
std::forward<Function>(function)
```

`std::forward` does not copy or move by itself. It returns a value category suitable for the next function to receive. This is useful here because `std::async` can accept and store the callable without restricting the API to lvalues or rvalues.

```cpp
CoroutineTask(CoroutineTask&& other) noexcept;
```

The `&&` in this move constructor has a different meaning: here, `CoroutineTask` is not a deduced template parameter, so this is an ordinary **rvalue reference**.

`std::make_shared<State>` owns an object containing the future. Shared ownership is used because a waiting `std::thread` and the coroutine's awaitable may both hold references to the future while work is in progress.

```cpp
[[nodiscard]] bool await_ready() const noexcept {
    return state_->future.wait_for(std::chrono::seconds(0)) ==
           std::future_status::ready;
}
```

`await_ready()` asks whether the coroutine should suspend immediately.

- It returns `true` when the associated `std::future` is ready.
- It returns `false` when work is still running.
- A zero-second wait performs a non-blocking status check.
- `std::future_status::ready` means the result is available.
- `[[nodiscard]]` warns if this protocol result is accidentally ignored.

```cpp
bool await_suspend(std::coroutine_handle<> handle) const {
    std::thread([state = state_, handle] {
        static_cast<void>(state->future.get());
        handle.resume();
    }).detach();
    return true;
}
```

When the work is not ready, `await_suspend()` arranges for the coroutine to resume later.

- The generic `std::coroutine_handle<>` identifies the suspended coroutine.
- A detached thread waits on `state->future`.
- `get()` blocks that helper thread until the result or exception is available.
- `handle.resume()` continues execution at the point after `co_await`.
- Returning `true` tells the coroutine runtime that the coroutine is suspended.
- `.detach()` allows the helper thread to finish independently, but it also means the helper thread must not access objects that have been destroyed.

`static_cast<void>(state->future.get())` explicitly discards the `shared_future` result because this helper thread only needs to wait and resume the coroutine. The actual result is read again by `await_resume()`.

```cpp
[[nodiscard]] TResult await_resume() const {
    return state_->future.get();
}
```

After waiting or immediate readiness, `await_resume()` retrieves the result stored in the future. It is called by the coroutine implementation when execution resumes after `co_await`.

```cpp
private:
    struct State {
        explicit State(std::future<TResult> future)
            : future(std::move(future)) {}

        std::shared_future<TResult> future;
    };

    std::shared_ptr<State> state_;
};
```

`State` owns a `std::shared_future`, which is used because both the awaitable and its detached waiting thread may need access. Unlike `std::future`, `std::shared_future` can be queried from multiple operations.

### Coroutine task type

```cpp
template <typename TResult>
class CoroutineTask {
public:
    struct promise_type {
```

`CoroutineTask<TResult>` is a custom task object. A coroutine's promise type controls creation, suspension, completion, and exception handling. The compiler finds `promise_type` as a nested member of the coroutine return type.

```cpp
CoroutineTask get_return_object() {
    return CoroutineTask{
        std::coroutine_handle<promise_type>::from_promise(*this)};
}
```

`get_return_object()` converts the promise back into the task object returned by a coroutine function. `from_promise()` creates a `coroutine_handle` that refers to the same coroutine frame.

```cpp
[[nodiscard]] std::suspend_always initial_suspend() noexcept {
    return {};
}
```

The coroutine starts in a suspended state. The caller controls the first execution through `start()`, instead of allowing the coroutine to begin automatically while the task is still being constructed.

```cpp
struct FinalAwaiter {
    [[nodiscard]] bool await_ready() noexcept {
        return false;
    }

    void await_suspend(
        std::coroutine_handle<promise_type> handle) noexcept {
        auto& promise = handle.promise();
        if (promise.error) {
            promise.result.set_exception(promise.error);
        } else {
            promise.result.set_value(std::move(promise.value));
        }
    }

    void await_resume() noexcept {}
};
```

`FinalAwaiter` controls coroutine completion:

- `await_ready()` returns `false`, so final suspension occurs.
- `await_suspend()` publishes either an exception or the stored result into the task's `std::promise`.
- `await_resume()` is empty because no caller value is needed after final completion.
- Moving `promise.value` transfers ownership into the promise result.

```cpp
[[nodiscard]] FinalAwaiter final_suspend() noexcept {
    return {};
}
```

This function defines final suspension behavior. The result can be made available before the caller destroys or joins the coroutine frame.

```cpp
void return_value(TResult result) {
    value = std::move(result);
}
```

`co_return result` calls `return_value()`. It stores the coroutine result in the promise so `FinalAwaiter` can publish it later.

```cpp
void unhandled_exception() {
    error = std::current_exception();
}
```

If the coroutine throws an exception that is not handled internally, the promise stores `std::current_exception()` for transfer to the caller through the future.

```cpp
std::promise<TResult> result;
TResult value{};
std::exception_ptr error;
```

These promise members store the future's result channel, successful value, and optional exception.

```cpp
using Handle = std::coroutine_handle<promise_type>;
```

This alias shortens the repeated coroutine-handle type.

```cpp
explicit CoroutineTask(Handle handle) : handle_(handle) {
    result_ = handle.promise().result.get_future();
}
```

The task constructor stores the coroutine handle and obtains a `std::future<TResult>` from the promise. The future lets `calcUsingCoroutines()` wait for the result.

The copy constructor and copy assignment operator are deleted, as they are for the thread pool. A coroutine task owns a coroutine handle, so copying it could allow multiple objects to destroy or resume the same coroutine frame.

The move constructor transfers the handle and future by moving and replaces the old handle with an empty handle using `std::exchange(other.handle_, Handle{})`. The destructor and move assignment also destroy the coroutine frame when this task object owns it.

```cpp
void start() {
    if (handle_ && !handle_.done()) {
        handle_.resume();
    }
}
```

`start()` resumes the manually suspended initial coroutine. `handle_` is converted to boolean by the coroutine handle implementation; it is true when the handle is valid. `done()` prevents restarting an already completed coroutine.

```cpp
[[nodiscard]] TResult get() {
    return result_.get();
}
```

`get()` blocks until the promise publishes a value or exception, then returns the `TResult` or rethrows the stored exception.

### Coroutine account function

```cpp
CoroutineTask<Result> processAccountCoroutine(
    const std::string& account,
    const OrderDetails& orders,
    int worker_id) {
    Result result = co_await AsyncAwaitable<Result>{
        [&account, &orders, worker_id] {
            const auto [lot_balance, error] =
                OrderProcessor::process(orders);
            if (error) {
                std::cerr << "Account " << account << " worker " << worker_id
                          << " error: " << *error << '\n';
            }
            return Result{worker_id, lot_balance, error};
        }};
    co_return result;
}
```

This function contains coroutine control keywords:

- `co_await AsyncAwaitable<Result>{...}` invokes the awaitable protocol while asynchronously processing the account.
- If the work is not ready, the coroutine suspends instead of blocking the caller.
- When resumed, the `Result` produced by `await_resume()` is assigned to `result`.
- `co_return result` publishes the result into the coroutine promise.
- The lambda captures account and order references and is executed by `std::async`.

### Coroutine calculation

```cpp
void calcUsingCoroutines(const OrderPerAccounts& orders_per_account) {
```

`calcUsingCoroutines()` creates one coroutine task for each account.

```cpp
std::vector<CoroutineTask<Result>> submitted;
submitted.reserve(orders_per_account.size());
```

It stores the coroutine tasks and reserves enough capacity for one task per account.

```cpp
int worker_id = 0;
for (const auto& [account, orders] : orders_per_account) {
    submitted.emplace_back(
        processAccountCoroutine(account, orders, worker_id));
    ++worker_id;
}
```

Each iteration creates a suspended account coroutine with a unique ID. Because `initial_suspend()` suspends immediately, adding a task does not begin account processing yet.

```cpp
for (auto& task : submitted) {
    task.start();
}
```

Starting each task resumes the initial suspension. Each coroutine calls `co_await` and launches its `AsyncAwaitable` work.

```cpp
int required_broker_limit = 0;
for (auto& task : submitted) {
    const Result result = task.get();
    if (!result.error) {
        required_broker_limit += result.lot_balance;
    }
}
```

The caller retrieves one future result per coroutine. `get()` waits until the async work is complete and the final awaiter has published the promise value.

```cpp
std::cout << "calcUsingCoroutines - required_broker_limit: "
          << required_broker_limit << '\n';
```

This prints the final coroutine-method broker limit. With the sample data it prints `10`.

## Main function

```cpp
int main() {
```

Defines the process entry point. The conventional return type is `int` because operating systems receive process exit status information from this value.

```cpp
using namespace concurrent_example;
```

Allows the unqualified names `buildDummyOrders()` and the four calculation functions to be found within this namespace.

It is commonly placed in a single executable source file; broad namespace imports in reusable headers are generally discouraged because they can introduce name collisions.

```cpp
const auto orders = buildDummyOrders();
```

Calls the sample builder and stores the result as a const value. `auto` deduces an `OrderPerAccounts`, and `const` prevents reassignment of the map variable.

It does not imply that nested objects are deeply immutable by the compiler: const references are used by each calculation function, preventing modification at that interface level.

```cpp
std::cout << "Using native worker channels\n";
calcUsingNative(orders);
```

Prints a section header and runs the native channel implementation.

```cpp
std::cout << "Using a fixed C++ thread pool\n";
calcUsingPool(orders);
```

Prints the second section header and runs the custom pool implementation.

```cpp
std::cout << "Using std::async with futures\n";
calcUsingAsync(orders);
```

Prints the third header and runs the standard-library async implementation.

```cpp
std::cout << "Using C++ coroutines with async work\n";
calcUsingCoroutines(orders);
```

Prints the fourth header and runs the coroutine implementation. Each account coroutine waits asynchronously through its custom awaitable and then contributes its result to the aggregate broker limit.

```cpp
return 0;
```

Returns zero from `main`, conventionally indicating successful execution.

## Modern C++ syntax used throughout the file

### Structured bindings

```cpp
const auto [lot_balance, error] = some_pair;
```

Introduced in C++17, structured binding decomposes the pair's members into separate variables. It is less repetitive than writing `some_pair.first` and `some_pair.second`.

The resulting variables have the same const/reference semantics associated with the structured-binding declaration.

### Range-based loops

```cpp
for (const auto& value : values)
```

This syntax iterates without manually maintaining an index or iterator. `const auto&` is preferred when no copy is needed and values must not be modified.

### Lambdas

```cpp
[captures](parameters) -> return_type {
    return value;
}
```

A lambda is a local anonymous function. In this file, captures are essential:

- `[this]` captures the thread pool object;
- `[worker_id, &jobs]` captures some values and some references;
- `[&orders]` shares the map loop entry with the task.

### RAII synchronization

`std::scoped_lock`, `std::unique_lock`, and mutex ownership use RAII. Lock objects acquire their resource in constructors and release it in destructors. Automatic cleanup is more reliable than manually calling `unlock()` at every return path.

### `std::optional`

`std::optional<T>` avoids sentinel values for ordinary absence. It is used here for receiving data after channel closure and representing an optional processing error.

Common operations include:

```cpp
if (value)                  // check whether it has data
value.has_value()           // explicit check
*value                      // get the data without adding optional handling
value.value()               // get the data or throw std::bad_optional_access
```

### `std::future`

A future represents a result that is not necessarily available yet. It can return a value or rethrow an exception. Typical operations are:

```cpp
std::future<Result> result;
const Result value = result.get();
result.wait();
```

`get()` waits for and consumes the result. It should be called once, unlike a reusable getter.

### C++ coroutines

C++20 coroutines use control keywords and awaitable protocol methods instead of a normal function call executing only from start to return:

- `co_await awaitable` suspends or synchronously resumes according to `await_ready()`;
- `co_return value` completes the coroutine with a result;
- `promise_type` customizes the coroutine's object creation, suspension points, result publication, and exception handling; and
- `std::coroutine_handle` identifies and resumes a suspended coroutine frame.

The awaitable protocol is:

```cpp
awaitable.await_ready();       // whether suspension can be skipped
awaitable.await_suspend(handle); // run when suspension is needed
awaitable.await_resume();      // run when the coroutine resumes
```

Returning `true` from `await_suspend()` keeps the coroutine suspended. A coroutine handle can be resumed with `handle.resume()`.

Coroutine support in this example also uses:

- `std::suspend_always` for manually controlled initial and final suspension;
- `std::promise<TResult>` and `std::future<TResult>` to expose the result;
- `std::exception_ptr` and `std::current_exception()` to transfer exceptions; and
- `std::exchange()` to move a coroutine handle while leaving the source empty.

### Why `[[nodiscard]]` is used

`[[nodiscard]]` communicates that discarding the return value is almost certainly a mistake. The compiler warning protects these operations:

- returning a calculated lot balance and an error;
- returning a future task result;
- returning generated sample orders; and
- returning a future that could expose task exceptions.
- returning coroutine readiness or suspension values that affect coroutine behavior.

Ignoring `process()` could hide a processing error. Ignoring `submit()` would lose access to the task's result. Ignoring `buildDummyOrders()` would discard deliberately built data.

It is not a runtime assertion. It only adds an intentional compiler diagnostic when a return value is ignored.

## Concurrency comparison

| Method | Thread management | Result delivery | Configuration | Best fit |
|---|---|---|---|---|
| Channel workers | Explicitly create and join threads | Shared send/receive channels | Manual worker lifetime | Learning producer/worker patterns and pipelines |
| Custom `ThreadPool` | Pool owns reusable worker threads | `std::future` with `std::packaged_task` | Fixed worker count | Repeated task submission with controlled concurrency |
| `std::async` | Standard-library launches tasks | `std::future` | Launch policy specified explicitly | Small independent asynchronous tasks |
| C++ coroutine example | Helper threads plus explicit coroutine resumption | `std::promise` and `std::future` | Custom promise, awaitable, and coroutine task | Learning the coroutine protocol without an external coroutine runtime |

For a small fixed set of account tasks, `std::async` is often the clearest implementation. A pool is better when many tasks are submitted repeatedly and a bounded worker count should be reused. Channels are useful when modeling producer-consumer pipelines or streaming work over time. Coroutines are useful for expressing suspend-and-resume operations as sequential-looking code, but a custom implementation must carefully manage coroutine lifetime, result publication, and exception transfer.

## Safety notes

- Always join every `std::thread` before its function exits to avoid `std::terminate` during thread-object destruction.
- Protect shared mutable state with mutexes or atomics; a channel or a future does not make surrounding application logic automatically safe.
- Do not modify or destroy captured references before async or worker tasks finish.
- Use `std::launch::async` when the current example explicitly requires actual asynchronous execution.
- Keep task execution outside pool mutex critical sections to avoid blocking submissions while expensive work runs.
- Use `std::optional` to model absence rather than guessing with sentinel integers.
- Keep coroutine handles and captured references alive until every coroutine has finished or resumed.
- Do not call `start()` twice on a coroutine that is already running or completed.
- Destroy or move a custom `CoroutineTask` according to its move-only ownership rules; never copy its handle.
- Detached helper threads are useful for this small example, but a production scheduler should usually own their lifetime and coroutine resumption.
- Store coroutine exceptions through the promise/future boundary instead of letting them escape unexpectedly across `co_await`.
- Exceptions thrown by pool tasks should be examined because some task-exception designs need to return an error state instead of a future exception.
