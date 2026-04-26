# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

Krpc is a C++11 distributed RPC framework built on:
- **Protobuf**: Service definition, serialization/deserialization
- **Muduo**: Multi-threaded epoll-based network I/O
- **Zookeeper**: Service registration and discovery
- **Glog**: Asynchronous logging

## Build Commands

### Initial build
```bash
mkdir -p build && cd build && cmake .. && make -j$(nproc)
```

### Rebuild (after code changes)
```bash
cd build && make -j$(nproc)
```

### Regenerate protobuf files (after editing .proto)
```bash
protoc --cpp_out=src src/Krpcheader.proto
protoc --cpp_out=example example/user.proto
```

### Run server / client
```bash
# Zookeeper must be running first: sudo systemctl start zookeeper
cd bin && ./server -i ./test.conf   # server
cd bin && ./client -i ./test.conf   # client
```

### Build and run benchmarks
```bash
cd build && cmake .. -DKRPC_BUILD_BENCHMARKS=ON && make -j$(nproc)

# Serialization benchmark (no server needed)
./bin/ser_bench

# RPC performance benchmark (requires server + ZK running)
./bin/rpc_bench -c ./bin/test.conf
./bin/rpc_bench -c ./bin/test.conf --concurrency 1,8,32 --requests 5000
```

## Architecture

The framework follows the classic Protobuf RPC pattern with three main phases:

### Core classes (`include/` + `src/`)

| Class | Role |
|-------|------|
| `KrpcApplication` | Singleton entry point. Initializes config via `Krpcconfig`. Call `Init()` first. |
| `KrpcProvider` | Server-side. Registers services via `NotifyService()`, starts a Muduo TcpServer, deserializes incoming RPC requests, dispatches to the correct method, and sends back responses. |
| `KrpcChannel` | Client-side. Inherits `google::protobuf::RpcChannel`. On `CallMethod()`, queries Zookeeper for the target service's IP:port, connects via TCP, serializes the request header (`KrpcHeader` proto) + body, sends it, and reads the response. |
| `KrpcController` | Inherits `google::protobuf::RpcController`. Tracks RPC call success/failure and error text. |
| `Krpcconfig` | Reads `.conf` files (key=value format). |
| `ZkClient` | Wrapper around the Zookeeper C client. Handles connection, node creation, and data retrieval. |
| `KrpcLogger` | RAII wrapper around glog with colored stderr output. |

### Wire protocol (`src/Krpcheader.proto`)

Every TCP message begins with a serialized `RpcHeader`:
- `service_name` — fully qualified service name
- `method_name` — RPC method to invoke
- `args_size` — byte length of the serialized request body

The header is sent first, followed by the protobuf-serialized request body. This framing solves TCP stream sticking (粘包).

### Service discovery flow

1. Server registers itself in Zookeeper under a path derived from the service name.
2. Client's `KrpcChannel::CallMethod()` queries Zookeeper to resolve the service name to an IP:port.
3. Client connects to that address and sends the RPC request.

### Example service (`example/user.proto`)

Defines `UserServiceRpc` with `Login` and `Register` methods. The server implementation is in `example/callee/Kserver.cc`, the client in `example/caller/Kclient.cc`.

## Code Style

- **Classes**: PascalCase with `Krpc` prefix (e.g. `KrpcProvider`)
- **Methods**: PascalCase (e.g. `NotifyService()`)
- **Member variables**: `m_` prefix + camelCase (e.g. `m_config`, `m_clientfd`)
- **Local variables**: snake_case
- **Header guards**: `#ifndef _ClassName_H`
- **Comments**: Chinese comments are common in this codebase
- **Indentation**: 4 spaces, opening braces on same line

## Dependencies

Installed system packages on Ubuntu: `protobuf-compiler`, `libprotobuf-dev`, `libzookeeper-mt-dev`, `zookeeperd`, `libgoogle-glog-dev`, `libgflags-dev`, `libboost-all-dev`, `muduo` (manual install).

## Config file format

`bin/test.conf` uses simple key=value pairs:
```
rpcserverip=127.0.0.1
rpcserverport=8000
zookeeperip=127.0.0.1
zookeeperport=2181
```

## Benchmarks

### `benchmark/serialization_bench.cc` (`bin/ser_bench`)
Compares Protobuf vs JSON (jsoncpp) serialization for `LoginRequest` and `RegisterRequest`:
- Serialized byte size
- Serialize/deserialize speed over 500K iterations

### `benchmark/rpc_bench.cc` (`bin/rpc_bench`)
End-to-end RPC performance tests (requires server running):
- Latency percentiles (P50/P95/P99) and QPS at configurable concurrency levels
- Connection reuse vs per-request connect overhead
- Zookeeper lookup and TCP connect isolation
