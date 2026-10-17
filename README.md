# C++ Conan Playground

This project is a small C++/Conan workspace for experimenting with modern C++ build setup, structured logging, and Kafka consumption using Redpanda-compatible `librdkafka`.

It contains two executable entry points:

- `hello_world`: a lightweight Quill-based logging demo
- `kafka_consumer`: a simple Kafka consumer that subscribes to a topic and prints incoming payloads

The project is intentionally minimal and uses Conan + CMake instead of a large framework, making it easy to understand and extend.

---

## Project layout

```text
.
├── CMakeLists.txt
├── README.md
├── reconfig.sh
├── requirements.txt
├── profiles/
│   ├── macos-x86_64
│   └── macos-x86_64-debug
├── build/
├── cmd/
│   ├── README.md
│   ├── helloworld/
│   │   └── main.cpp
│   └── domain1/
│       └── consumer.cpp
├── internal/
│   └── common/
│       └── kafka/
│           └── kafka.hpp
└── .vscode/
```

---

## Prerequisites

- Python 3
- CMake
- Ninja or Make
- Conan 2
- A running Kafka/Redpanda broker at `127.0.0.1:9092`

The current consumer code expects a topic named `orders_topic` in a local broker setup.

---

## 1) Create and activate a Python environment

From the project root:

```bash
python3 -m venv venv
source venv/bin/activate
python -m pip install --upgrade pip
```

On Windows PowerShell:

```powershell
py -m venv venv
.\venv\Scripts\Activate.ps1
python -m pip install --upgrade pip
```

---

## 2) Install project dependencies

```bash
python -m pip install -r requirements.txt
```

This installs the Python-side tooling used to manage the Conan profile and dependency setup.

---

## 3) Detect or configure Conan profiles

A default profile can be detected automatically:

```bash
conan profile detect --force
```

The repo also ships with project profiles under the `profiles/` directory, including:

- `profiles/macos-x86_64`
- `profiles/macos-x86_64-debug`

Example profile content:

```ini
[settings]
os=Macos
arch=x86_64
build_type=Release
compiler=apple-clang
compiler.version=15
compiler.cppstd=20
compiler.libcxx=libc++
```

---

## 4) Build the project

This workspace includes a helper script that regenerates the Conan environment and rebuilds the CMake project:

```bash
bash reconfig.sh
```

This script uses the default profile at `profiles/macos-x86_64` and writes outputs under `build/release`.

To build a debug configuration:

```bash
bash reconfig.sh ./profiles/macos-x86_64-debug
```

This writes to `build/debug`.

You can also force the output folder explicitly:

```bash
bash reconfig.sh ./profiles/macos-x86_64 debug
```

---

## 5) Run the executables

After the build succeeds, run the generated binaries from the project root:

```bash
./build/release/hello_world
./build/release/kafka_consumer
```

### hello_world

This sample uses Quill and writes structured JSON log output to stdout.

### kafka_consumer

This sample subscribes to the `orders_topic` topic and logs received messages from the configured Kafka/Redpanda broker.

If you are using a local Redpanda instance, the consumer expects it to be reachable at:

```text
127.0.0.1:9092
```

## Run unit tests

The Kafka wrapper tests use GoogleTest and do not require a running broker:

```bash
ctest --test-dir build/release --output-on-failure
```

### Generate Kafka unit-test coverage

On macOS with Apple Clang, use the coverage mode of the helper script. It configures an isolated Debug build, builds the tests, runs them, prints a line-coverage summary for `kafka.hpp`, and creates an HTML report:

```bash
bash reconfig.sh coverage
```

Open `build/coverage/coverage-html/index.html` to inspect the annotated report. LLVM coverage tools (`llvm-profdata` and `llvm-cov`) are provided by Xcode Command Line Tools.

---

## 6) Example build flow

If you want to do the steps manually instead of using the helper script:

```bash
conan install . \
  --profile:host=profiles/macos-x86_64 \
  --profile:build=profiles/macos-x86_64 \
  --output-folder=build/release \
  --build=missing

cmake -S . -B build/release \
  -DCMAKE_TOOLCHAIN_FILE=build/release/conan_toolchain.cmake \
  -DCMAKE_BUILD_TYPE=Release

cmake --build build/release
```

---

## 7) Notes on the sample apps

The code is intentionally compact and follows a simple structure:

- `cmd/helloworld/main.cpp` demonstrates structured logging
- `cmd/domain1/consumer.cpp` demonstrates Kafka consumption
- `internal/common/kafka/kafka.hpp` contains the reusable consumer wrapper

This project is a good base for adding more command-line services or additional Kafka producers/consumers.

---

## Troubleshooting

- If Conan cannot find dependencies, make sure the profile matches your host operating system and compiler.
- If the Kafka consumer fails to connect, verify the broker is running and the port is correct.
- If the build fails, rerun the project generator script to refresh the Conan toolchain and CMake configuration.

```bash
bash reconfig.sh
```

---

## Related files

- [`CMakeLists.txt`](CMakeLists.txt)
- [`reconfig.sh`](reconfig.sh)
- [`cmd/README.md`](cmd/README.md)
- [`profiles/macos-x86_64`](profiles/macos-x86_64)
