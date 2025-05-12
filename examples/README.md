# 示例程序

本目录包含分布式实时日志分析与监控系统的主要示例程序。

## 核心示例程序

1. **complete_system_example.cpp** - 完整系统示例，演示整个系统的运行流程
   - 包括日志收集、处理、分析、存储和告警等功能
   - 这是最常用的示例程序，用于启动整个系统

2. **processor_example.cpp** - 日志处理器示例，演示日志处理流程
   - 包括日志解析、过滤和转发等功能

3. **storage_example.cpp** - 存储组件示例，演示日志存储功能
   - 包括MySQL和Redis存储的使用方法

4. **test_tcp_server.cpp** - TCP服务器测试，用于测试网络通信功能
5. **test_tcp_client.cpp** - TCP客户端测试，用于测试网络通信功能

## 如何运行

### 运行完整系统示例

```bash
cd build
make complete_system_example
./bin/complete_system_example
```

### 生成测试日志

```bash
cd build
make log_generator_example
./bin/log_generator_example --rate 10 --count 100 --network --target 127.0.0.1:8001
```

## 其他示例程序

不常用的示例程序已移至 [legacy](./legacy) 目录。 