# 分布式实时日志分析与监控系统

## 项目描述
设计并实现了一个基于C++的高性能分布式实时日志分析与监控系统。该系统由日志采集器模块、日志处理器（Processor）、日志存储器（Storage）、日志分析器（Analyzer）和告警系统（Alert）五大核心模块组成。采用模块化设计，实现了日志的实时采集、处理、存储和分析功能，特别适用分布式下的日志监控需求。系统具有高可用性、可扩展性和低延迟的特点。

## 核心职责
1. 系统架构设计
   - 设计并实现了基于C++的分布式日志处理系统
   - 采用模块化架构，实现日志采集、处理、存储和分析的解耦
   - 基于muduo网络库实现高性能的TCP通信
   - 使用Docker容器化部署，提升系统可移植性

2. 核心功能实现
   - 日志采集模块：实现基于内存池和线程池的高效日志收集
   - 实时处理模块：基于无锁队列实现高性能的消息传输
   - 存储模块：设计Redis+MySQL的混合存储方案，实现热数据和冷数据的分离存储
   - 告警模块：实现基于规则的告警系统，支持自定义告警规则

3. 性能优化
   - 实现自定义内存池，优化内存分配与回收
   - 使用无锁队列实现多生产者单消费者模式，提升并发处理能力
   - 采用线程池优化并行任务处理
   - 实现高效的数据压缩和传输机制

## 技术亮点
1. 高性能设计
   - 基于muduo网络库实现高性能TCP服务器
   - 使用内存池和无锁队列优化内存管理
   - 实现高效的线程池调度机制

2. 可靠性保证
   - 实现日志数据的可靠传输和存储
   - 设计故障恢复机制
   - 完善的日志记录和监控

3. 工程实践
   - 规范的代码风格和注释
   - 完善的单元测试
   - 详细的文档编写（用户手册、代码阅读指南等）

## 技术栈
- **核心语言**：C++11/14/17
- **网络框架**：muduo网络库
- **数据存储**：
  - Redis：高性能缓存和实时数据存储
  - MySQL：持久化和历史数据存储
- **库与组件**：
  - nlohmann/json：高性能JSON解析库，用于数据序列化和配置
  - zlib：数据压缩库，用于日志内容压缩
  - libcurl：网络库，用于webhook告警通知
  - uuid：用于生成唯一标识符
  - 正则表达式库：用于日志解析和规则匹配
- **自研组件**：
  - 内存池：高效内存管理
  - 无锁队列：高性能线程间通信
  - 线程池：并行任务调度
- **构建与部署**：
  - CMake：跨平台构建系统
  - Docker：容器化部署
- **开发与测试**：
  - VSCode：开发环境
  - GDB/LLDB：调试工具
  - Google Test：单元测试框架

## 项目成果
1. 系统性能
   - 实现了高效的日志收集和处理
   - 支持实时数据分析和告警
   - 系统具有良好的可扩展性

2. 工程价值
   - 提供了完整的日志监控解决方案
   - 系统模块化设计便于维护和扩展
   - 完善的文档支持，便于团队协作

## 个人收获
1. 深入理解了C++高性能编程
2. 掌握了分布式系统设计原则
3. 提升了系统架构设计能力
4. 加强了工程化实践能力

## 技术亮点与实现难点

1. 高可用性实现
   - **多层次异常处理与重试机制**：所有关键操作均实现完善的异常捕获和重试逻辑，确保单点故障不影响整体服务
   ```cpp
   // 日志收集器中的重试机制
   void LogCollector::HandleRetry(const std::vector<LogEntry>& logs) {
       threadPool_->Submit([this, logs]() {
           for (uint32_t attempt = 0; attempt < config_.maxRetryCount; ++attempt) {
               std::this_thread::sleep_for(config_.retryInterval);
               if (!isActive_) break;
               
               std::vector<LogEntry> retryLogs = logs;
               if (SendLogBatch(retryLogs)) return;  // 成功则返回
           }
           // 失败处理...
       });
   }
   ```

   - **存储冗余设计**：关键数据同时写入Redis和MySQL，即使单一存储系统故障，也能保证数据可用性
   ```cpp
   // 告警数据双重存储实现
   void AlertManager::SaveAlert(const Alert& alert) {
       // Redis存储
       if (redisStorage_) {
           try {
               std::string key = "alert:" + alert.id;
               redisStorage_->Set(key, AlertToJson(alert));
               // 更多Redis操作...
           } catch (const std::exception& e) { /* 错误处理 */ }
       }
       
       // MySQL存储
       if (mysqlStorage_) {
           try {
               storage::MySQLStorage::LogEntry entry;
               entry.id = alert.id;
               entry.fields["alert_data"] = AlertToJson(alert);
               mysqlStorage_->SaveLogEntry(entry);
           } catch (const std::exception& e) { /* 错误处理 */ }
       }
   }
   ```

2. 可扩展性实现
   - **规则引擎的插件化设计**：分析器和告警系统支持动态添加、删除和修改规则，无需修改核心代码
   ```cpp
   // 分析器的可扩展规则引擎
   void LogAnalyzer::AddRule(std::shared_ptr<AnalysisRule> rule) {
       if (rule) {
           std::lock_guard<std::mutex> lock(rulesMutex_);
           rules_.push_back(rule);
           
           // 添加到规则组
           const auto& group = rule->GetConfig().group;
           ruleGroups_[group].push_back(rule);
           
           // 按优先级排序
           SortRulesByPriority();
       }
   }
   ```

   - **通知渠道的可扩展架构**：支持多种通知渠道（邮件、Webhook等），并可通过统一接口扩展
   ```cpp
   // 通知渠道的抽象接口
   class NotificationChannel {
   public:
       virtual bool SendAlert(const Alert& alert) = 0;
       virtual std::string GetName() const = 0;
       virtual std::string GetType() const = 0;
       virtual ~NotificationChannel() = default;
   };
   
   // 在告警管理器中注册新渠道
   void AlertManager::AddNotificationChannel(
       std::shared_ptr<NotificationChannel> channel) {
       std::lock_guard<std::mutex> lock(channelsMutex_);
       channels_.push_back(std::move(channel));
   }
   ```

3. 低延迟实现
   - **内存池和无锁队列优化**：使用自定义内存池和无锁队列，减少内存分配和线程同步开销
   ```cpp
   // 内存池初始化
   LogCollector::LogCollector(const CollectorConfig& config) : isActive_(false) {
       // 预分配内存减少运行时开销
       memoryPool_ = std::make_unique<common::MemoryPool>(
           sizeof(LogEntry), config_.memoryPoolSize);
       // 其他初始化...
   }
   ```

   - **批量处理与异步设计**：所有模块采用批量处理和异步设计，通过一次处理多条日志减少单位处理开销
   ```cpp
   // 日志处理器中的批量处理
   void LogProcessor::ProcessThreadFunc() {
       while (running_) {
           std::vector<LogRecord> batch;
           // 获取一批待处理记录
           {
               std::lock_guard<std::mutex> lock(recordsMutex_);
               size_t count = std::min(config_.batchSize, pendingRecords_.size());
               if (count > 0) {
                   batch.insert(batch.end(), pendingRecords_.begin(), 
                               pendingRecords_.begin() + count);
                   pendingRecords_.erase(pendingRecords_.begin(), 
                                        pendingRecords_.begin() + count);
               }
           }
           
           // 批量异步处理
           for (const auto& record : batch) {
               threadPool_->Submit([this, record]() {
                   this->ProcessRecord(record);
               });
           }
       }
   }
   ```

   - **冷热分层存储**：实时数据存入Redis，历史数据归档到MySQL，确保热点数据查询的低延迟响应
   ```cpp
   // 存储层的冷热分离实现
   bool LogProcessor::StoreLogRecord(const LogRecord& record) {
       // 热数据存入Redis（低延迟）
       if (redisStorage_) {
           try {
               std::string key = "log:" + record.id;
               redisStorage_->HashSet(key, "content", record.message);
               redisStorage_->Expire(key, config_.redisTTL);
           } catch (...) { /* 错误处理 */ }
       }
       
       // 冷数据归档到MySQL（持久存储）
       if (mysqlStorage_) {
           try {
               storage::MySQLStorage::LogEntry entry;
               // 构建MySQL日志条目...
               mysqlStorage_->SaveLogEntry(entry);
           } catch (...) { /* 错误处理 */ }
       }
       
       return true;
   }
   ```