# Lock-Free Ring Queue

`lockfreequeue.hpp` defines the `LockFreeQueue<T>` template. It is a
bounded ring-buffer queue backed by a fixed-size `std::vector<T>`.
Elements are copied into the buffer; no heap allocation occurs during
`enqueue()` or `dequeue()`.

## Data structure

The queue has the following state:

- `size_`: the total number of slots in the ring
- `buffer_`: a `std::vector<T>` containing queued elements
- `head_`: an atomic index for the next element to dequeue
- `tail_`: an atomic index for the next element to enqueue

```cpp
#include "lockfreequeue.hpp"

LockFreeQueue<int> queue(1024);
```

Indexes wrap to the beginning of the ring using:

```text
index = (index + 1) % size_
```

## Capacity

The implementation reserves one slot to distinguish between an empty
queue and a full queue. Therefore, a ring constructed with `size` slots
can store at most `size - 1` elements.

For example:

```text
LockFreeQueue<int> queue(8); // Holds at most 7 elements
```

The constructor does not validate its argument. `size == 0` causes
modulo-by-zero in `enqueue()` and `dequeue()`, while `size == 1`
leaves no usable element slot.

## Enqueue

```cpp
if (!queue.enqueue(42)) {
    // The queue is full.
}
```

`enqueue(const T&)`:

1. Loads the current `tail_`.
2. Calculates the next tail index.
3. Returns `false` if the next tail equals `head_`.
4. Otherwise copies the item into `buffer_[tail]`.
5. Advances `tail_` and returns `true`.

## Dequeue

```cpp
int item = 0;

if (queue.dequeue(item)) {
    // item contains the oldest queued value.
}
```

`dequeue(T&)`:

1. Loads the current `head_`.
2. Returns `false` if `head_ == tail_`.
3. Otherwise copies `buffer_[head]` into the output reference.
4. Advances `head_` and returns `true`.

A successful dequeue does not destroy the object stored in the buffer
slot or clear it. That slot remains occupied until a producer overwrites
it with another element.

## Atomic memory ordering

The head and tail indexes use `std::atomic<size_t>` so that their stores
and loads can be observed across threads without the undefined behavior
of an ordinary unsynchronized data race.

Memory ordering answers two separate questions:

- **What** value can another thread observe from the atomic operation?
- **When** must surrounding ordinary memory operations become visible
  relative to that atomic operation?

It does not mean that processor operations happen in wall-clock order.
It defines an allowed synchronization relationship between threads.

### `memory_order_relaxed`

```cpp
size_t tail = tail_.load(memory_order_relaxed);
size_t head = head_.load(memory_order_relaxed);
```

- **What:** Loads or stores the atomic value only. It provides atomicity
  and modification-order guarantees, but no synchronization with
  surrounding memory operations.
- **Why:** Each thread already owns its local progression of an index.
  The producer is the writer of `tail_`, and the consumer is the writer
  of `head_`. The local read is only used to calculate the next write.
- **When:** Use it when another thread only needs to observe the atomic
  integer value and not that ordinary data such as a `buffer_` element
  has been published.
- **Who:** The producer normally reads `tail_` relaxed, while the
  consumer normally reads `head_` relaxed.
- **How:** The load reads the atomic variable but does not establish an
  acquire relationship. Do not expect it by itself to make a buffer
  write visible.

### `memory_order_acquire`

```cpp
if (next_tail == head_.load(memory_order_acquire)) return false;
if (head == tail_.load(memory_order_acquire)) return false;
```

- **What:** An acquire load makes preceding writes in a synchronizing
  release/acquire relationship visible to the executing thread. It
  establishes a **reader side** of publication.
- **Why:** The producer must observe enough consumer state to detect
  queue fullness. The consumer must observe enough producer state to
  detect queue availability and see the stored element.
- **When:** Use acquire loads on the index owned by another thread when
  that load protects or publishes ordinary data.
- **Who:** The producer uses it when reading `head_`; the consumer uses
  it when reading `tail_`.
- **How:** The acquire load pairs with a release store of the same
  atomic variable. Once the acquire observes the released value, the
  thread may safely observe writes sequenced before that store.

Examples in this queue:

- Consumer loads `tail_` acquire after the producer stored it release.
  This synchronization permits the consumer to read the element written
  to `buffer_[tail]`.
- Producer loads `head_` acquire after the consumer stored it release.
  This permits the producer to reuse the slot after the consumer's read.

An acquire load by itself does not release anything to other threads.

### `memory_order_release`

```cpp
tail_.store(next_tail, memory_order_release);
head_.store((head + 1) % size_, memory_order_release);
```

- **What:** A release store publishes ordinary writes made before the
  store.
- **Why:** The index is the public signal that a ring slot is ready for
  use. Storing the index before the buffer operation would allow the
  other thread to use an incomplete element.
- **When:** Use release when ordinary writes must be visible to a thread
  after that thread performs a matching acquire load.
- **Who:** The producer releases the new `tail_` after writing the
  element. The consumer releases the new `head_` after reading the slot.
- **How:** Write the data first, then store the advanced index with
  release semantics.

For example:

```text
Producer writes buffer_[tail]
        -> release store tail_
Consumer acquire loads tail_
        -> consumer safely reads buffer_[tail]
```

A release store does not mean that earlier writes become atomic. It
means that threads synchronizing through the matching atomic operation
must observe those writes in the defined relationship.

### `memory_order_acq_rel`

This queue does not use `memory_order_acq_rel`.

- **What:** Combines acquire behavior for a read with release behavior
  for a write in a read-modify-write operation.
- **Why it may be needed:** An algorithm that both reads current state
  and publishes new state in one atomic operation, such as a compare-and-
  swap, commonly needs acquire and release together.
- **When:** Use it for read-modify-write operations that must synchronize
  data in both directions.
- **Who:** Typically a general queue implementation rather than a unique
  producer or consumer would use this with a CAS.
- **How:** Replace an atomic read followed by a conditional write with
  an atomic compare-and-swap. The CAS can use `acq_rel` if it both
  consumes and publishes state.

This implementation does not use compare-and-swap, so this mode is not
currently needed.

### `memory_order_seq_cst`

- **What:** Gives the atomic operation acquire/release-like ordering plus
  one global total order shared by sequentially consistent operations.
- **Why:** It makes reasoning about multiple threads and several atomic
  variables simpler.
- **When:** Use it as a safe default when the performance cost is
  acceptable or when relaxed ordering is difficult to justify.
- **Who:** It could be chosen by producers, consumers, or library users
  that require the strongest behavior.
- **How:** Pass it explicitly or omit the order from a store/load; stores,
  loads, and read-modify-write operations then use
  `memory_order_seq_cst`.

This queue deliberately uses weaker ordering because its index protocol
is clearer than a general MPMC protocol:

- relaxed for the writer's own index
- acquire for an index owned by another thread
- release for publishing that index

### `memory_order_consume`

`memory_order_consume` is an acquire-like dependency ordering mode. It
is designed to synchronize only through data-dependent reads. It is
error-prone and is effectively treated as acquire by current mainstream
C++ implementations, so it should not be used in this queue.

### Quick comparison

| Memory order | What | Used here |
|---|---|---|
| `relaxed` | Atomic value only; no synchronization | Own index reads |
| `acquire` | Observes writes published before a matching release | Opposite index reads |
| `release` | Publishes preceding ordinary writes | Advances `tail_` or `head_` |
| `acq_rel` | Acquires and releases in one read-modify-write | Not used |
| `seq_cst` | Strong acquire/release behavior plus global atomic order | Not used explicitly |
| `consume` | Dependency-based acquire; deprecated from practical use | Not used |

## Important limitations

The ordering rules above support the intended single-producer,
single-consumer protocol. They do not make the queue safe for multiple
producers or consumers:

1. Both index updates can lose one another in a concurrent read/check/
   write sequence.
2. Multiple producers can write the same slot.
3. Multiple consumers can read the same slot.
4. The code has no index compare-and-swap or per-slot state machine.

Memory ordering reduces ordinary-data races, but it cannot turn an SPSC
algorithm into a correct MPMC algorithm by itself.

## Thread-safety

The implementation is intended for one producer and one consumer (SPSC):

- One thread may call `enqueue()`.
- Another thread may call `dequeue()`.
- The producer and consumer may run concurrently.

Concurrent calls to `enqueue()` by multiple producers or to `dequeue()`
by multiple consumers are not supported. The implementation uses a
read/check/write sequence without compare-and-swap operations, so
multiple callers can update or overwrite the ring incorrectly.

For a multiple-producer or multiple-consumer queue, use external
synchronization or replace the index-update logic with a correctly
reviewed MPMC lock-free algorithm.