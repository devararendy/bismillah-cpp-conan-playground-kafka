# Order Book

The executable maintains separate order books by stock code. Each book matches new
orders using price-time priority:

- The best price executes first.
  - Buys match the lowest-priced sell order.
  - Sells match the highest-priced buy order.
- Orders at the same price execute in arrival order (FIFO).
- A resting order is filled until it is empty or no matching price remains.
- Unfilled quantity remains in the order book.

## Data model

Each order event has:

| Field | Description |
|---|---|
| UserID | User that submitted the event |
| StockCode | Stock identifier and order book key |
| Price | Positive numeric limit price |
| Qty | Positive order quantity |
| Side | `Buy` or `Sell` |
| EventType | `New` or `Cancel` |
| OrderID | Optional client-provided identifier for the order |

Each price level contains:

- `Price`: the limit price.
- `Qty`: the total remaining quantity of all orders at that price.

## Why these containers are used

### `std::map` for price levels

The bids and asks use `std::map<Price, std::deque<Order>, ...>`:

```cpp
std::map<Price, std::deque<Order>, std::greater<Price>> bids_;
std::map<Price, std::deque<Order>> asks_;
```

`std::map` keeps prices sorted. This makes matching efficient because the executable
does not need to search the entire book for the best bid or ask. It is also natural
for operations such as adding a new price level and removing an empty level.

The `map` member is ordered automatically:

- The asks use the default comparison `std::less<Price>`, so their lowest price is
  the first element.
- The bids use `std::greater<Price>`, so their highest price is the first element.

`std::greater<Price>` is a comparison function object. For example:

```cpp
std::greater<Price>{}(101, 100) // true
```

It is passed as the third template argument to `bids_` so `map` sorts entries from
highest to lowest. Because matching always starts with `bids_.begin()` or
`asks_.begin()`, it obtains the best available price immediately.

`std::map` is a balanced search tree. Typical insertion, lookup, and removal cost is
`O(log n)` for `n` price levels.

### `std::deque` for orders at one price

Each price level maps to a `std::deque<Order>` containing the orders at that exact
price:

```cpp
std::deque<Order>
```

A `std::deque` (double-ended queue) provides:

- Fast insertion at the back, where a newly arriving order belongs.
- Fast removal from the front, where the oldest resting order executes or cancels.
- Stable FIFO (first in, first out) price-time priority.

For example, three buy orders at price `101` are processed as:

```text
front                              back
b1 (arrived first) -> b2 -> b3 (arrived last)
```

The matching code always consumes `queue.front()`. When the first order is filled,
it is removed; the next order becomes the new front. Therefore, `std::deque` is a
better fit than repeatedly inserting or erasing in the middle of a vector.

### `std::unordered_map` for order lookup

The book stores an index of active orders:

```cpp
std::unordered_map<OrderId, Order*>
```

`std::unordered_map` maps an `OrderId` directly to its corresponding `Order`. It is
used by cancel processing and when filled orders are removed.

An unordered map does not keep keys in sorted order, but it does not need to. Its
main goal is fast lookup by an arbitrary order ID rather than lookup by price.
Average lookup, insertion, and removal cost is `O(1)`.

Use `std::unordered_map` when key order is irrelevant and the operation is a direct
lookup, such as finding an order from `OrderId`. Use `std::map` when the keys need
to be sorted, such as finding the highest bid or lowest ask. Although both maps
support `find()`, `std::unordered_map` has average `O(1)` lookup while `std::map`
has predictable `O(log n)` lookup and maintains key order.

| Container | Key | Purpose |
|---|---|---|
| `std::map` bids | Descending price | Find the highest bid quickly |
| `std::map` asks | Ascending price | Find the lowest ask quickly |
| `std::deque` | Arrival position | Preserve FIFO priority at one price |
| `std::unordered_map` | Order ID | Find or remove an active order quickly |

## Matching behavior

New orders return:

- `Accepted` when no execution occurs.
- `Accepted` with one or more `Executed` events when all or part executes.
- `Rejected` for invalid fields, an invalid stock, zero quantity, or a duplicate order ID.

Cancel events return:

- `Canceled` when an active order is found and removed.
- `Rejected` when the order does not exist.

Execution output has this format:

```text
Executed,StockCode,ExecutionPrice,Quantity,BuyOrderID,SellOrderID
```

## Run

Build and run the executable:

```bash
cmake --build build/release --target orderbook
./build/release/orderbook AAPL
```

Type CSV events on standard input. The format is:

```text
user_id,stock_code,price,qty,side,event_type,order_id(optional)
```

Example input:

```text
u1,AAPL,100,10,Sell,New,s1
u2,AAPL,101,6,Buy,New,b1
u1,AAPL,100,4,Sell,Cancel,s1
```

Pipe the same input from a shell:

```bash
printf 'u1,AAPL,100,10,Sell,New,s1\nu2,AAPL,101,6,Buy,New,b1\n' |
  ./build/release/orderbook AAPL
```

Example output:

```text
Accepted,new order accepted
Bids:
Asks:
  100 10
Accepted,new order accepted and executed
Executed,AAPL,100,6,b1,s1
Bids:
Asks:
  100 4
```

The executable only processes events whose `StockCode` matches the stock code passed
as its command-line argument.