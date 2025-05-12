# 遗留示例程序

本目录包含一些不常用的示例程序，这些程序主要用于测试和演示系统的各个组件。在日常使用中，您可以专注于主目录中的核心示例程序。

## 目录中的示例程序

1. **analyzer_example.cpp** - 演示日志分析器组件的使用方法
2. **alert_example.cpp** - 演示告警管理器组件的使用方法
3. **storage_factory_example.cpp** - 演示存储工厂模式的使用方法
4. **log_collector_example.cpp** - 演示日志收集器组件的使用方法
5. **network_example.cpp** - 演示网络通信组件的使用方法

## 如何使用

如果需要单独测试某个组件，可以使用以下命令编译并运行相应的示例程序：

```bash
cd build
make analyzer_example  # 或其他示例程序名称
./bin/analyzer_example
```

注意：这些示例程序可能需要先启动相关的服务（如MySQL、Redis等）才能正常运行。 