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

---

For the full project setup and dependency instructions, see the main project README at [README.md](../README.md).
