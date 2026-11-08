# Command entry points

This directory contains the runnable command-line programs for the project.

## Overview

```text
cmd/
├── README.md
├── helloworld/
│   └── main.cpp
└── domain1/
    └── consumer.cpp
```

## 1) hello_world

Source: `cmd/helloworld/main.cpp`

This small example demonstrates:

- CMake project wiring
- Quill logging setup
- Structured JSON log output to stdout

Run it after building the project:

```bash
./build/release/hello_world
```

It creates a logger, emits structured log events, and prints JSON output to the terminal.

## 2) kafka_consumer

Source: `cmd/domain1/consumer.cpp`

This sample connects to a Kafka or Redpanda broker and subscribes to the `orders_topic` topic.

The program expects:

- a broker at `127.0.0.1:9092`
- a topic named `orders_topic`
- a running Kafka-compatible cluster

Run it after building the project:

```bash
./build/release/kafka_consumer
```

During execution it will:

- initialize a Kafka consumer
- subscribe to the configured topic
- poll for messages in a loop
- print payloads and metadata as messages arrive

Use `Ctrl+C` to exit the loop gracefully.

## Shared Kafka implementation

The shared consumer logic lives in:

- `internal/common/kafka/kafka.hpp`

This file encapsulates the consumer configuration, subscription logic, and polling loop used by `cmd/domain1/consumer.cpp`.

## Building the executables

From the project root:

```bash
bash reconfig.sh
```

This regenerates the Conan build environment and compiles both binaries under `build/release` by default.

For a debug build:

```bash
bash reconfig.sh ./profiles/macos-x86_64-debug
```

## Typical workflow

```bash
bash reconfig.sh
./build/release/hello_world
./build/release/kafka_consumer
```

## 3) orderbook

Source: `cmd/orderbook/orderbook.cpp`

Run a stock-specific order book and send CSV events on standard input:

```bash
printf 'u1,AAPL,100,10,Sell,New,s1\nu2,AAPL,101,6,Buy,New,b1\n' | ./build/release/orderbook AAPL
```

The CSV format is:

```text
user_id,stock_code,price,qty,side,New|Cancel,order_id(optional)
```

New orders are accepted and added to price-time priority queues. Crossing orders produce `Executed` output. A cancellation can use the generated order ID from the optional CSV field, or omit it to cancel the first active matching order for that user, side, and price.

---

For the full project setup and dependency instructions, see the main project README at [README.md](../README.md).
