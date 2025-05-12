# 分布式实时日志分析与监控系统使用手册

## 目录

1. [系统概述](#1-系统概述)
2. [环境要求](#2-环境要求)
3. [安装指南](#3-安装指南)
4. [系统架构](#4-系统架构)
5. [核心功能](#5-核心功能)
6. [使用指南](#6-使用指南)
7. [配置说明](#7-配置说明)
8. [故障排除](#8-故障排除)
9. [常见问题](#9-常见问题)
10. [附录](#10-附录)

## 1. 系统概述

分布式实时日志分析与监控系统是一个高性能、可扩展的日志处理平台，专为大规模分布式系统设计。系统能够实时收集、处理、分析和存储各种应用程序和系统生成的日志数据，并提供实时监控、告警和分析功能。

### 1.1 主要特点

- **高性能**：采用多线程和异步处理技术，能够高效处理大量日志数据
- **实时分析**：支持实时日志分析和监控，快速识别系统异常
- **分布式架构**：支持水平扩展，适应不同规模的系统需求
- **多样化存储**：支持MySQL和Redis等多种存储方式
- **灵活配置**：提供丰富的配置选项，适应不同场景需求
- **告警机制**：内置告警系统，及时通知系统异常

### 1.2 应用场景

- 大型分布式系统的日志管理
- 微服务架构的监控与故障排查
- 安全事件检测与分析
- 业务数据分析与统计
- 系统性能监控与优化

## 2. 环境要求

### 2.1 硬件要求

- **CPU**：建议4核或以上
- **内存**：建议8GB或以上
- **存储**：根据日志量决定，建议50GB以上
- **网络**：千兆网络环境

### 2.2 软件要求

- **操作系统**：Ubuntu 20.04 LTS或更高版本
- **编译器**：GCC 9.3.0或更高版本
- **构建工具**：CMake 3.16或更高版本
- **依赖库**：
  - MySQL客户端库
  - Redis客户端库
  - ZLib压缩库
  - UUID生成库
  - cURL网络库
  - Muduo网络库
  - nlohmann/json库

### 2.3 第三方服务

- **MySQL**：版本8.0或更高
- **Redis**：版本6.0或更高

## 3. 安装指南

### 3.1 安装依赖

```bash
# 更新软件包列表
sudo apt update

# 安装编译工具和基本依赖
sudo apt install -y build-essential cmake git

# 安装必要的库
sudo apt install -y libmysqlclient-dev libhiredis-dev zlib1g-dev uuid-dev libcurl4-openssl-dev

# 安装Muduo网络库依赖
sudo apt install -y libboost-dev libboost-test-dev libboost-program-options-dev libevent-dev
```

### 3.2 获取源代码

```bash
# 克隆代码仓库
git clone https://github.com/yourusername/Distributed-Real-time-Log-Analysis-and-Monitoring-System.git
cd Distributed-Real-time-Log-Analysis-and-Monitoring-System
```

### 3.3 编译系统

```bash
# 创建并进入构建目录
mkdir -p build
cd build

# 配置CMake
cmake ..

# 编译
make -j$(nproc)
```

### 3.4 设置数据库

#### 3.4.1 MySQL配置

```bash
# 登录MySQL
mysql -u root -p

# 创建数据库
CREATE DATABASE log_analysis;

# 创建日志表
USE log_analysis;
CREATE TABLE IF NOT EXISTS log_entries (
    id VARCHAR(64) PRIMARY KEY,
    timestamp VARCHAR(32) NOT NULL,
    level VARCHAR(16) NOT NULL,
    source VARCHAR(128) NOT NULL,
    message TEXT NOT NULL,
    created_at TIMESTAMP DEFAULT CURRENT_TIMESTAMP
) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;

# 创建用户并授权
CREATE USER 'loguser'@'localhost' IDENTIFIED BY 'logpass';
GRANT ALL PRIVILEGES ON log_analysis.* TO 'loguser'@'localhost';
FLUSH PRIVILEGES;
```

#### 3.4.2 Redis配置

确保Redis服务已启动并运行：

```bash
# 安装Redis（如果尚未安装）
sudo apt install -y redis-server

# 启动Redis服务
sudo systemctl start redis-server

# 检查Redis状态
sudo systemctl status redis-server
```

## 4. 系统架构

### 4.1 核心组件

系统由以下核心组件组成：

1. **日志收集器（Collector）**：负责从各种来源收集日志数据
2. **日志处理器（Processor）**：处理和转换日志数据
3. **日志分析器（Analyzer）**：分析日志内容，识别模式和异常
4. **存储管理器（Storage）**：管理日志的存储和检索
5. **告警管理器（Alert）**：监控系统状态并生成告警
6. **网络通信（Network）**：处理组件间的通信

### 4.2 数据流

![系统数据流](https://example.com/dataflow.png)

1. 日志源生成日志数据
2. 收集器接收并预处理日志
3. 处理器解析和标准化日志
4. 分析器执行实时分析
5. 存储管理器将日志保存到数据库
6. 告警管理器监控异常并触发告警

## 5. 核心功能

### 5.1 日志收集

- 支持多种日志来源：文件、网络、应用API等
- 支持多种日志格式：JSON、文本、自定义格式
- 支持日志压缩和批处理
- 提供可靠的传输机制

### 5.2 日志处理

- 日志解析和结构化
- 字段提取和转换
- 日志过滤和路由
- 日志增强（添加元数据）

### 5.3 日志分析

- 实时模式识别
- 异常检测
- 统计分析
- 趋势分析

### 5.4 日志存储

- 支持MySQL关系型数据库
- 支持Redis缓存
- 支持日志轮转和归档
- 提供高效的查询接口

### 5.5 告警管理

- 基于规则的告警机制
- 多级告警策略
- 告警聚合和去重
- 多渠道通知（可扩展）

### 5.6 Web仪表盘

- 实时系统状态监控
- 日志搜索和查询
- 可视化报表和图表
- 告警管理界面

## 6. 使用指南

### 6.1 启动系统

使用完整系统示例程序启动整个系统：

```bash
cd build
./bin/complete_system_example
```

系统启动后，会自动连接配置的MySQL和Redis服务，并开始监听TCP连接（默认端口8001）。

### 6.2 生成测试日志

使用日志生成器工具生成测试日志：

```bash
cd build
./bin/log_generator_example --rate 10 --count 100 --network --target 127.0.0.1:8001
```

参数说明：
- `--rate`：每秒生成的日志数量
- `--count`：总共生成的日志数量
- `--network`：通过网络发送日志
- `--target`：目标服务器地址和端口

### 6.3 查看日志分析结果

运行分析脚本生成分析报告：

```bash
cd build
./analyze_logs.sh
```

分析报告将保存在`analysis`目录下，文件名格式为`log_analysis_final_YYYYMMDD_HHMMSS.txt`。

### 6.4 启动Web仪表盘

启动API服务器和Web仪表盘：

```bash
cd build
python3 api_server.py
```

然后在浏览器中访问`http://localhost:5000`即可查看Web仪表盘。

## 7. 配置说明

### 7.1 系统配置

系统的主要配置参数在代码中定义，可以通过修改相应的配置对象来调整系统行为。

#### 7.1.1 MySQL配置

```cpp
xumj::storage::MySQLConfig mysqlConfig;
mysqlConfig.host = "127.0.0.1";
mysqlConfig.port = 3306;
mysqlConfig.username = "root";
mysqlConfig.password = "yourpassword";
mysqlConfig.database = "log_analysis";
mysqlConfig.poolSize = 5;
```

#### 7.1.2 Redis配置

```cpp
xumj::storage::RedisConfig redisConfig;
redisConfig.host = "127.0.0.1";
redisConfig.port = 6379;
redisConfig.password = "";
redisConfig.database = 0;
redisConfig.poolSize = 5;
```

#### 7.1.3 网络配置

```cpp
xumj::network::TcpServerConfig serverConfig;
serverConfig.host = "0.0.0.0";
serverConfig.port = 8001;
serverConfig.threadCount = 4;
```

#### 7.1.4 日志处理器配置

```cpp
xumj::processor::LogProcessorConfig processorConfig;
processorConfig.batchSize = 100;
processorConfig.flushInterval = 5;  // 秒
processorConfig.maxQueueSize = 10000;
```

### 7.2 日志级别

系统支持以下日志级别：

- `DEBUG`：调试信息，用于开发和排错
- `INFO`：一般信息，记录正常操作
- `WARNING`：警告信息，可能的问题但不影响系统运行
- `ERROR`：错误信息，表示出现了问题
- `CRITICAL`：严重错误，可能导致系统部分功能不可用

### 7.3 告警配置

告警规则可以通过告警管理器配置：

```cpp
xumj::alert::AlertConfig alertConfig;
alertConfig.errorRateThreshold = 0.05;  // 错误率阈值
alertConfig.responseTimeThreshold = 500;  // 响应时间阈值（毫秒）
alertConfig.checkInterval = 60;  // 检查间隔（秒）
```

## 8. 故障排除

### 8.1 常见错误

#### 8.1.1 数据库连接失败

**症状**：系统启动时报告"无法连接到MySQL数据库"错误

**解决方案**：
1. 确认MySQL服务是否正在运行：`sudo systemctl status mysql`
2. 检查数据库连接配置是否正确
3. 确认数据库用户权限：`SHOW GRANTS FOR 'loguser'@'localhost';`
4. 检查防火墙设置是否阻止了连接

#### 8.1.2 Redis连接失败

**症状**：系统报告"无法连接到Redis服务器"错误

**解决方案**：
1. 确认Redis服务是否正在运行：`sudo systemctl status redis-server`
2. 检查Redis连接配置是否正确
3. 确认Redis没有设置密码或密码正确
4. 检查Redis绑定地址是否允许连接

#### 8.1.3 TCP服务器启动失败

**症状**：系统报告"无法启动TCP服务器"错误

**解决方案**：
1. 确认指定端口是否被其他程序占用：`netstat -tuln | grep 8001`
2. 尝试更改服务器端口
3. 检查系统防火墙设置
4. 确认用户有足够权限绑定端口

### 8.2 日志文件

系统运行时会生成以下日志文件，可用于故障排除：

- `debug_direct.log`：直接处理的日志记录
- `system_output.log`：系统运行输出日志

### 8.3 调试模式

启用调试模式以获取更详细的日志信息：

```cpp
// 在代码中设置日志级别为DEBUG
xumj::common::Logger::SetLogLevel(xumj::common::LogLevel::DEBUG);
```

## 9. 常见问题

### 9.1 系统性能问题

**问题**：系统处理日志速度变慢

**解决方案**：
1. 增加处理器线程数
2. 调整批处理大小
3. 优化数据库查询
4. 考虑增加服务器资源

### 9.2 内存使用过高

**问题**：系统占用内存过多

**解决方案**：
1. 减小队列大小
2. 调整批处理参数
3. 优化对象创建和销毁
4. 检查是否存在内存泄漏

### 9.3 日志丢失

**问题**：部分日志没有被正确处理或存储

**解决方案**：
1. 检查网络连接稳定性
2. 增加重试机制
3. 调整队列大小和处理超时
4. 启用日志持久化

## 10. 附录

### 10.1 命令行参数

#### 10.1.1 完整系统示例

```
./bin/complete_system_example [选项]

选项:
  --help                  显示帮助信息
  --config <file>         指定配置文件路径
  --port <number>         指定TCP服务器端口（默认：8001）
  --threads <number>      指定工作线程数（默认：4）
  --mysql-host <host>     MySQL主机地址（默认：127.0.0.1）
  --mysql-port <port>     MySQL端口（默认：3306）
  --redis-host <host>     Redis主机地址（默认：127.0.0.1）
  --redis-port <port>     Redis端口（默认：6379）
```

#### 10.1.2 日志生成器

```
./bin/log_generator_example [选项]

选项:
  --help                  显示帮助信息
  --rate <number>         每秒生成的日志数（默认：10）
  --count <number>        总共生成的日志数（默认：100）
  --file <filename>       输出到文件（默认：generated_logs.txt）
  --network               通过网络发送日志
  --target <host:port>    目标服务器地址和端口（默认：127.0.0.1:8001）
  --json                  生成JSON格式日志
  --batch <number>        批处理大小（默认：10）
```

### 10.2 API参考

系统提供以下REST API：

#### 10.2.1 获取统计信息

```
GET /api/stats
```

返回系统基本统计信息。

#### 10.2.2 获取日志级别分布

```
GET /api/log_levels
```

返回日志级别分布统计。

#### 10.2.3 获取日志来源分布

```
GET /api/log_sources
```

返回日志来源分布统计。

#### 10.2.4 获取错误趋势

```
GET /api/error_trend
```

返回错误日志趋势数据。

#### 10.2.5 获取最新日志

```
GET /api/latest_logs
```

返回最新的20条日志记录。

#### 10.2.6 获取告警信息

```
GET /api/alerts
```

返回最新的告警信息。

#### 10.2.7 生成分析报告

```
POST /api/generate_report
```

触发生成新的分析报告。

### 10.3 数据库结构

#### 10.3.1 log_entries表

| 字段名 | 类型 | 说明 |
|--------|------|------|
| id | VARCHAR(64) | 主键，日志唯一标识 |
| timestamp | VARCHAR(32) | 日志生成时间戳 |
| level | VARCHAR(16) | 日志级别 |
| source | VARCHAR(128) | 日志来源 |
| message | TEXT | 日志内容 |
| created_at | TIMESTAMP | 记录创建时间 |

### 10.4 示例代码

#### 10.4.1 发送日志示例

```cpp
#include <xumj/network/tcp_client.h>
#include <string>
#include <iostream>

int main() {
    // 创建TCP客户端
    xumj::network::TcpClient client("127.0.0.1", 8001);
    
    // 连接服务器
    if (!client.Connect()) {
        std::cerr << "无法连接到服务器" << std::endl;
        return 1;
    }
    
    // 构造日志消息
    std::string logMessage = R"({
        "id": "test-log-1",
        "timestamp": "2023-05-10T12:34:56Z",
        "level": "INFO",
        "source": "test-client",
        "message": "这是一条测试日志"
    })";
    
    // 发送日志
    if (!client.Send(logMessage + "\n")) {
        std::cerr << "发送日志失败" << std::endl;
        return 1;
    }
    
    std::cout << "日志发送成功" << std::endl;
    return 0;
}
```

#### 10.4.2 查询日志示例

```cpp
#include <xumj/storage/mysql_storage.h>
#include <iostream>

int main() {
    // 配置MySQL连接
    xumj::storage::MySQLConfig config;
    config.host = "127.0.0.1";
    config.port = 3306;
    config.username = "root";
    config.password = "yourpassword";
    config.database = "log_analysis";
    
    // 创建MySQL存储对象
    xumj::storage::MySQLStorage mysql(config);
    
    // 测试连接
    if (!mysql.TestConnection()) {
        std::cerr << "无法连接到MySQL数据库" << std::endl;
        return 1;
    }
    
    // 查询最近的错误日志
    auto errorLogs = mysql.QueryLogEntriesByLevel("ERROR", 10, 0);
    std::cout << "最近的错误日志：" << std::endl;
    for (const auto& log : errorLogs) {
        std::cout << "ID: " << log.id << ", 时间: " << log.timestamp
                  << ", 来源: " << log.source << ", 消息: " << log.message << std::endl;
    }
    
    return 0;
}
```

### 10.5 联系与支持

如有问题或需要支持，请联系：

- 项目维护者：[您的名字]
- 电子邮件：[您的邮箱]
- GitHub仓库：[仓库链接] 