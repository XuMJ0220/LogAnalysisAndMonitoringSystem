# 日志分析系统使用文档

## 1. 项目介绍

本项目是一个完整的日志收集、处理、分析和告警系统。该系统可以从各种来源收集日志，进行解析和处理，分析日志内容以识别模式和异常，并在必要时触发告警。系统还支持将日志和分析结果存储到Redis和MySQL数据库中。

## 2. 系统架构

系统由以下几个核心模块组成：

- **日志收集器(LogCollector)**: 负责从各种来源收集日志
- **日志处理器(LogProcessor)**: 负责解析和处理原始日志数据
- **日志分析器(LogAnalyzer)**: 负责分析日志内容，识别模式和异常
- **告警管理器(AlertManager)**: 负责根据分析结果生成和管理告警
- **存储模块(Storage)**: 负责将日志和分析结果存储到Redis和MySQL中

各模块之间的数据流向如下：

```
日志来源 → 日志收集器 → 日志处理器 → 日志分析器 → 告警管理器
                                  ↓
                               存储模块
```

## 3. 环境要求

- C++17或更高版本
- CMake 3.10或更高版本
- Redis服务器(可选，用于数据存储)
- MySQL服务器(可选，用于数据存储)
- Hiredis库(Redis客户端)
- MySQL客户端库
- Google Test和Google Mock(用于测试)
- pthread库(线程支持)

## 4. 安装与编译

### 4.1 安装依赖

在Ubuntu/Debian系统上：

```bash
sudo apt-get update
sudo apt-get install -y build-essential cmake libhiredis-dev libmysqlclient-dev libgtest-dev
```

### 4.2 编译项目

```bash
# 创建构建目录
mkdir -p build
cd build

# 配置项目
cmake ..

# 编译项目
make -j4

# 运行测试
./bin/run_tests

# 运行示例程序
./bin/processor_example
```

## 5. 使用指南

### 5.1 日志收集器(LogCollector)

日志收集器负责从各种来源收集日志数据并进行初步过滤。

#### 基本用法

```cpp
#include "xumj/collector/log_collector.h"

using namespace xumj::collector;

// 创建收集器配置
CollectorConfig config;
config.batchSize = 100;                           // 批处理大小
config.flushInterval = std::chrono::seconds(5);   // 刷新间隔
config.compressLogs = true;                       // 是否压缩日志
config.minLevel = LogLevel::INFO;                 // 最小日志级别

// 创建日志收集器
LogCollector collector(config);

// 提交日志
collector.SubmitLog("这是一条信息日志", LogLevel::INFO);
collector.SubmitLog("这是一条警告日志", LogLevel::WARNING);

// 批量提交日志
std::vector<std::string> logs = {"日志1", "日志2", "日志3"};
collector.SubmitLogs(logs, LogLevel::ERROR);

// 手动刷新日志缓冲区
collector.Flush();
```

#### 添加过滤器

```cpp
// 添加级别过滤器(只处理INFO及以上级别的日志)
collector.AddFilter(std::make_shared<LevelFilter>(LogLevel::INFO));

// 添加关键字过滤器(过滤包含特定关键字的日志)
collector.AddFilter(std::make_shared<KeywordFilter>(
    std::vector<std::string>{"error", "exception"}, false));

// 清除所有过滤器
collector.ClearFilters();
```

#### 设置回调

```cpp
// 设置错误回调
collector.SetErrorCallback([](const std::string& error) {
    std::cerr << "收集器错误: " << error << std::endl;
});

// 设置提交回调
collector.SetSubmitCallback([](const std::vector<LogEntry>& entries) {
    std::cout << "提交了 " << entries.size() << " 条日志" << std::endl;
    return true;
});
```

### 5.2 日志处理器(LogProcessor)

日志处理器负责解析和处理原始日志数据。

#### 基本用法

```cpp
#include "xumj/processor/log_processor.h"

using namespace xumj::processor;

// 创建处理器配置
ProcessorConfig config;
config.threadPoolSize = 4;                          // 线程池大小
config.batchSize = 50;                              // 批处理大小
config.maxQueueSize = 1000;                         // 最大队列大小
config.processInterval = std::chrono::seconds(1);   // 处理间隔

// 创建日志处理器
LogProcessor processor(config);

// 启动处理器
processor.Start();

// 提交日志数据
LogData logData;
logData.id = "log-123";
logData.data = "这是一条测试日志";
logData.source = "test-client";
logData.timestamp = std::chrono::system_clock::now();

processor.SubmitLogData(logData);

// 等待处理完成后停止处理器
std::this_thread::sleep_for(std::chrono::seconds(2));
processor.Stop();
```

#### 添加自定义解析器

```cpp
// 创建自定义解析器
class MyLogParser : public LogParser {
public:
    LogRecord Parse(const LogData& data) const override {
        LogRecord record;
        record.id = data.id;
        record.timestamp = std::chrono::system_clock::to_time_t(data.timestamp);
        record.level = "INFO";
        record.message = data.data;
        record.source = data.source;
        return record;
    }
    
    std::string GetName() const override {
        return "MyParser";
    }
    
    bool CanParse(const LogData& data) const override { 
        return true; 
    }
};

// 添加到处理器
processor.AddParser(std::make_shared<MyLogParser>());
```

#### 设置回调

```cpp
// 设置处理回调
processor.SetProcessCallback([](const std::string& id, bool success) {
    std::cout << "日志处理" << (success ? "成功" : "失败") << "：ID = " << id << std::endl;
});
```

### 5.3 日志分析器(LogAnalyzer)

日志分析器负责分析日志内容，识别模式和异常。

#### 基本用法

```cpp
#include "xumj/analyzer/log_analyzer.h"

using namespace xumj::analyzer;

// 创建分析器配置
AnalyzerConfig config;
config.threadPoolSize = 2;                           // 线程池大小
config.batchSize = 10;                               // 批处理大小
config.analyzeInterval = std::chrono::seconds(1);    // 分析间隔
config.storeResults = true;                          // 是否存储结果

// 创建日志分析器
LogAnalyzer analyzer(config);

// 启动分析器
analyzer.Start();

// 提交日志记录进行分析
LogRecord record;
record.id = "log-1";
record.timestamp = "2023-01-01 10:00:00";
record.level = "ERROR";
record.source = "server1";
record.message = "Database connection error occurred";

analyzer.SubmitRecord(record);

// 等待分析完成后停止
std::this_thread::sleep_for(std::chrono::seconds(2));
analyzer.Stop();
```

#### 添加分析规则

```cpp
// 添加正则表达式规则
auto regexRule = std::make_shared<RegexAnalysisRule>(
    "ErrorRegexRule",
    "error|exception|failed",
    std::vector<std::string>{"has_error"}
);
analyzer.AddRule(regexRule);

// 添加关键字分析规则
auto keywordRule = std::make_shared<KeywordAnalysisRule>(
    "PerformanceRule",
    std::vector<std::string>{"slow", "timeout", "performance"},
    true
);
analyzer.AddRule(keywordRule);
```

#### 设置回调

```cpp
// 设置分析完成回调
analyzer.SetAnalysisCallback([](const std::string& recordId, 
                              const std::unordered_map<std::string, std::string>& results) {
    std::cout << "分析完成：日志ID = " << recordId << std::endl;
    for (const auto& [key, value] : results) {
        std::cout << "  " << key << " = " << value << std::endl;
    }
});
```

### 5.4 告警管理器(AlertManager)

告警管理器负责根据分析结果生成和管理告警。

#### 基本用法

```cpp
#include "xumj/alert/alert_manager.h"

using namespace xumj::alert;
using namespace xumj::analyzer;

// 创建告警管理器配置
AlertManagerConfig config;
config.threadPoolSize = 2;
config.checkInterval = std::chrono::seconds(1);
config.resendInterval = std::chrono::seconds(5);
config.suppressDuplicates = true;

// 创建告警管理器
AlertManager manager(config);

// 启动告警管理器
manager.Start();

// 创建日志记录和分析结果
LogRecord record;
record.id = "log-test";
record.timestamp = "2023-01-01 10:00:00";
record.level = "WARNING";
record.source = "server1";
record.message = "CPU usage is high";

std::unordered_map<std::string, std::string> results;
results["cpu_usage"] = "85.0";

// 触发告警检查
auto triggerIds = manager.CheckAlerts(record, results);

// 等待处理完成后停止
std::this_thread::sleep_for(std::chrono::seconds(2));
manager.Stop();
```

#### 添加告警规则

```cpp
// 添加阈值规则
auto thresholdRule = std::make_shared<ThresholdAlertRule>(
    "HighCpuRule",
    "CPU使用率过高",
    "cpu_usage",
    80.0,
    ">=",
    AlertLevel::WARNING
);
manager.AddRule(thresholdRule);

// 添加关键字规则
std::vector<std::string> keywords = {"error", "failure", "critical"};
auto keywordRule = std::make_shared<KeywordAlertRule>(
    "ErrorKeywordRule",
    "包含错误关键字",
    "message",
    keywords,
    false,
    AlertLevel::ERROR
);
manager.AddRule(keywordRule);
```

#### 设置回调和处理告警

```cpp
// 设置告警回调
manager.SetAlertCallback([](const std::string& alertId, AlertStatus status) {
    std::cout << "告警ID: " << alertId << ", 状态: " << AlertStatusToString(status) << std::endl;
});

// 获取活跃告警
auto activeAlerts = manager.GetActiveAlerts();

// 解决告警
if (!activeAlerts.empty()) {
    manager.ResolveAlert(activeAlerts[0].id);
}
```

### 5.5 存储模块(Storage)

存储模块负责将日志和分析结果存储到Redis和MySQL数据库中。

#### Redis存储

```cpp
#include "xumj/storage/redis_storage.h"
#include "xumj/storage/storage_factory.h"

using namespace xumj::storage;

// 创建Redis配置
RedisConfig config;
config.host = "localhost";
config.port = 6379;
config.password = "password";
config.database = 0;
config.timeout = 1000;
config.poolSize = 3;

// 创建Redis存储
auto redisStorage = StorageFactory::CreateRedisStorage(config);

// 使用Redis存储
redisStorage->Set("test_key", "test_value");
std::string value = redisStorage->Get("test_key");
std::cout << "Redis值: " << value << std::endl;
```

#### MySQL存储

```cpp
#include "xumj/storage/mysql_storage.h"
#include "xumj/storage/storage_factory.h"

using namespace xumj::storage;

// 创建MySQL配置
MySQLConfig config;
config.host = "localhost";
config.port = 3306;
config.username = "user";
config.password = "password";
config.database = "logs";
config.poolSize = 3;

// 创建MySQL存储
auto mysqlStorage = StorageFactory::CreateMySQLStorage(config);

// 初始化MySQL表结构
mysqlStorage->Initialize();

// 保存日志条目
MySQLStorage::LogEntry entry;
entry.id = "log-123";
entry.timestamp = "2023-01-01T10:00:00Z";
entry.level = "ERROR";
entry.source = "app_server_01";
entry.message = "Database connection failed";
entry.fields["app"] = "backend";

mysqlStorage->SaveLogEntry(entry);
```

## 6. 示例程序

项目中包含了一个日志处理器示例程序，位于`examples/processor_example.cpp`。这个示例展示了如何创建和使用日志处理器和分析器。

要运行示例程序：

```bash
cd build
./bin/processor_example
```

## 7. 故障排除

### 7.1 连接数据库失败

如果连接到Redis或MySQL数据库失败：

- 确保数据库服务器正在运行
- 检查配置中的主机名、端口、用户名和密码是否正确
- 验证网络连接是否通畅
- 检查数据库用户是否有足够的权限

### 7.2 编译错误

如果编译过程中出现错误：

- 确保安装了所有必需的依赖项
- 检查CMake版本是否符合要求
- 验证编译器是否支持C++17

### 7.3 测试失败

如果测试失败：

- 检查是否缺少必要的环境配置
- 对于需要数据库连接的测试，确保数据库服务器可用
- 查看测试输出以获取更详细的错误信息 