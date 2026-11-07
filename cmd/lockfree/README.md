# Lock-Free Queue Example

This example demonstrates a bounded ring-buffer queue shared by two threads:

- `std::jthread producer` creates values from `0` to `19`.
- `std::jthread consumer` removes all 20 values.
- `LockFreeQueue<int>` coordinates the threads without mutexes or condition variables.

Source files:

- [lockfree.cpp](./lockfree.cpp)
- [lockfreequeue.hpp](../../internal/common/lockfreequeue/lockfreequeue.hpp)

## Build and run

Build the repository and run the `lockfree` executable:

```bash
bash reconfig.sh
./build/release/lockfree
```

The output contains each produced and consumed value, but the ordering across
the two threads is not deterministic.

## Includes

```cpp
#include "../../internal/common/lockfreequeue/lockfreequeue.hpp"
```

Includes the queue implementation. The relative path goes from `cmd/lockfree`
to `internal/common/lockfreequeue`.

```cpp
#include <iostream>
#include <thread>
```

- `<iostream>` provides `std::cout` for the sample output.
- `<thread>` provides `std::jthread`.

## Queue construction

```cpp
LockFreeQueue<int> q(5);
```

Creates a queue with capacity `5` for integers. Internally, the queue is a
fixed-size `std::vector<T>` used as a ring buffer.

The implementation reserves one slot to distinguish a full queue from an empty
queue. With a capacity of 5, the queue can hold at most 4 items.

## Producer thread

```cpp
std::jthread producer([&]() {
    for (int i = 0; i < 20; ++i) {
        while (!q.enqueue(i)) {}
        std::cout << "Produced: " << i << std::endl;
    }
});
```

### Lambda capture: `[&]`

`[&]` is a lambda capture list. `std::jthread` receives the lambda as the
function the new thread will execute, and `[&]` tells the lambda to capture
variables from its surrounding scope by reference.

```cpp
std::jthread producer([&]() {
    // q is used inside the lambda
});
```

Here, `[&]` allows the producer lambda to use `q` directly instead of copying
it or passing it explicitly. That matters because both the producer and the
consumer must share the same queue object.

`()` is the lambda parameter list. An empty list means the producer takes no
arguments. The full body is:

```text
capture list  parameter list  function body
 [&]           ()             { ... }
```

In contrast, `[=]` would capture variables by value, and `[&q]` would capture
only `q` by reference. This example could use `[&q]`, but `[&]` also captures
any other local variables that the lambda might later use.

For these particular lambdas, `[&]` and `[&q]` have effectively the same
result:

```cpp
std::jthread producer([&q]() {
    // q is accessible and refers to the original queue
});
```

The producer also declares loop variable `i` locally, while the rest of the
lambda uses only `q`. Therefore, `[&q]` is narrower and makes that dependency
explicit. The difference is:

- `[&]` captures every variable from the surrounding scope by reference that
  the lambda uses.
- `[&q]` captures only `q` by reference.
- `[=]` captures every variable used by the lambda by value. Copying `q` would
  copy its vector and atomic positions, so it would not be suitable here.

Because `q` is captured by reference, `producer` and `consumer` must not outlive
`q`. In this program, `q` is created first, and both threads are automatically
joined before `main` returns, so their lifetime is safely shorter than `q`.

Capture by reference means each lambda accesses the original `q`; it does not
create a copy of the queue, atomics, or vector. The queue itself provides the
required synchronization for the shared head and tail positions.

The producer creates values in ascending order and captures `q` by reference,
so it operates on the same queue as the consumer.

`enqueue` may reject an item when the queue is full. The `while (!q.enqueue(i))`
loop performs busy waiting: it retries until space becomes available. After a
successful enqueue, the producer prints the value.

## Consumer thread

```cpp
std::jthread consumer([&]() {
    for (int i = 0; i < 20; ++i) {
        int value;
        while (!q.dequeue(value)) {}
        std::cout << "Consumed: " << value << std::endl;
    }
});
```

The consumer repeatedly calls `dequeue` until it receives one of the 20 values.
If the queue is empty, the loop retries with busy waiting. The extracted value is
stored in `value` and printed.

## Queue implementation

The queue stores its next producer position in `tail_` and its next consumer
position in `head_`:

```cpp
atomic<size_t> head_;
atomic<size_t> tail_;
```

Each position advances cyclically using modulo arithmetic based on `size_`.

### Enqueue

```cpp
size_t tail = tail_.load(std::memory_order_relaxed);
size_t next_tail = (tail + 1) % size_;

if (next_tail == head_.load(std::memory_order_acquire)) return false;

buffer_[tail] = item;
tail_.store(next_tail, std::memory_order_release);
```

- The current tail is loaded without ordering overhead.
- `next_tail` wraps to zero when the producer reaches the end of the buffer.
- The queue is full when advancing the tail would reach the consumer's head.
- The item is written before publishing the new tail.
- `memory_order_release` publishes the new tail after the buffer write.
- The consumer's `memory_order_acquire` load observes that publication.

### Dequeue

```cpp
size_t head = head_.load(std::memory_order_relaxed);

if (head == tail_.load(std::memory_order_acquire)) return false;

item = buffer_[head];
head_.store((head + 1) % size_, std::memory_order_release);
```

- The current head is loaded by the consumer.
- If head equals tail, the queue is empty.
- The value at the head position is copied out.
- The head is advanced and published with release ordering.

## Thread lifetime and join behavior

Both threads use `std::jthread`, which is an automatically joining thread. The
producer and consumer variables remain in scope until `main` returns. At that
point, each thread is joined if it has not already finished, ensuring that all
20 values are produced and consumed before the program exits.

## Concurrency limitations

This sample is designed as a single-producer/single-consumer ring buffer. It is
not safe to call `enqueue` concurrently from multiple producers or `dequeue`
concurrently from multiple consumers without additional synchronization.

Other implementation considerations:

- Busy waiting continuously consumes CPU while the queue is empty or full.
- A capacity argument of zero is invalid because modulo and ring-buffer indexing
  would not have a valid size.
- The example is lock-free in the sense that it uses atomic head/tail positions
  and does not use locks, but it does not have formal support for multiple
  producers or multiple consumers.