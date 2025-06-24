#include <gtest/gtest.h>
#include <chrono>
#include <vector>
#include <string>
#include "xumj/collector/log_collector.h"
#include <thread>

using namespace xumj::collector;

// 假设LogCollector和相关依赖已正确包含和链接

TEST(LogCollectorBatchTest, BatchProcessingPerformance) {
    // 配置批量大小和线程池等参数
    CollectorConfig config;
    config.batchSize = 100;
    config.memoryPoolSize = 1024;
    config.threadPoolSize = 2;
    config.maxQueueSize = 10000;
    config.minLevel = LogLevel::INFO;
    config.compressLogs = false;
    config.enableRetry = false;

    LogCollector collector(config);
    collector.Initialize(config);

    const int logCount = 5000;
    std::vector<std::string> logs(logCount, "test log for batch performance");

    auto start = std::chrono::high_resolution_clock::now();
    for (const auto& log : logs) {
        collector.SubmitLog(log, LogLevel::INFO);
    }
    collector.Flush(); // 强制批量处理
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    // 断言：批量处理应在合理时间内完成（如1秒内）
    EXPECT_LT(duration, 1000);
}

TEST(LogCollectorBatchTest, BatchProcessingCorrectness) {
    CollectorConfig config;
    config.batchSize = 10;
    config.memoryPoolSize = 128;
    config.threadPoolSize = 1;
    config.maxQueueSize = 100;
    config.minLevel = LogLevel::INFO;
    config.compressLogs = false;
    config.enableRetry = false;

    LogCollector collector(config);
    collector.Initialize(config);

    // 提交20条日志
    for (int i = 0; i < 20; ++i) {
        collector.SubmitLog("log entry " + std::to_string(i), LogLevel::INFO);
    }
    collector.Flush();

    // 检查队列已清空
    EXPECT_EQ(collector.GetPendingCount(), 0);
}

TEST(LogCollectorBatchTest, SingleVsBatchPerformance) {
    using namespace xumj::collector;
    // 单条处理
    CollectorConfig config1;
    config1.batchSize = 1;
    config1.memoryPoolSize = 1024;
    config1.threadPoolSize = 2;
    config1.maxQueueSize = 200000;
    config1.minLevel = LogLevel::INFO;
    config1.compressLogs = false;
    config1.enableRetry = false;
    LogCollector collector1(config1);
    collector1.Initialize(config1);

    const int logCount = 10000; // 临时减少日志量，加快测试
    std::vector<std::string> logs(logCount, "test log single");

    auto start1 = std::chrono::high_resolution_clock::now();
    for (const auto& log : logs) {
        collector1.SubmitLog(log, LogLevel::INFO);
        std::this_thread::sleep_for(std::chrono::microseconds(50)); // 临时减少延迟，加快测试
    }
    // 循环Flush直到队列为空
    while (collector1.GetPendingCount() > 0) {
        collector1.Flush();
    }
    auto end1 = std::chrono::high_resolution_clock::now();
    auto duration1 = std::chrono::duration_cast<std::chrono::milliseconds>(end1 - start1).count();

    // 批量处理
    CollectorConfig config2;
    config2.batchSize = 100;
    config2.memoryPoolSize = 1024;
    config2.threadPoolSize = 2;
    config2.maxQueueSize = 200000;
    config2.minLevel = LogLevel::INFO;
    config2.compressLogs = false;
    config2.enableRetry = false;
    LogCollector collector2(config2);
    collector2.Initialize(config2);

    auto start2 = std::chrono::high_resolution_clock::now();
    for (const auto& log : logs) {
        collector2.SubmitLog(log, LogLevel::INFO);
        std::this_thread::sleep_for(std::chrono::microseconds(50)); // 临时减少延迟，加快测试
    }
    while (collector2.GetPendingCount() > 0) {
        collector2.Flush();
    }
    auto end2 = std::chrono::high_resolution_clock::now();
    auto duration2 = std::chrono::duration_cast<std::chrono::milliseconds>(end2 - start2).count();

    std::cout << "[大数据量+模拟IO延迟] 单条处理耗时: " << duration1 << " ms, 批量处理耗时: " << duration2 << " ms" << std::endl;
    EXPECT_LT(duration2, duration1); // 批量处理应更快
} 