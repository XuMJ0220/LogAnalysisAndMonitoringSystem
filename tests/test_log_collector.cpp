#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <memory>
#include "xumj/collector/log_collector.h"

using namespace xumj::collector;
using ::testing::_;
using ::testing::Return;

// 模拟回调类，用于验证提交是否被调用
class MockSubmitCallback {
public:
    MOCK_METHOD1(OnSubmit, bool(const std::vector<LogEntry>&));
};

// 测试日志收集器配置
TEST(LogCollectorTest, Configuration) {
    CollectorConfig config;
    config.batchSize = 100;
    config.flushInterval = std::chrono::milliseconds(500);
    config.compressLogs = true;
    config.maxRetryCount = 3;
    config.retryInterval = std::chrono::milliseconds(200);
    config.minLevel = LogLevel::INFO;
    
    LogCollector collector(config);
    
    // 验证配置是否正确设置
    EXPECT_EQ(collector.GetPendingCount(), 0U);
}

// 测试日志过滤器
TEST(LogCollectorTest, Filtering) {
    CollectorConfig config;
    LogCollector collector(config);
    
    // 添加过滤器，过滤掉DEBUG级别的日志
    collector.AddFilter(std::make_shared<LevelFilter>(LogLevel::INFO));
    
    // 添加过滤器，过滤掉包含"ignore"的日志
    collector.AddFilter(std::make_shared<KeywordFilter>(std::vector<std::string>{"ignore"}, true));
    
    // 清除过滤器
    collector.ClearFilters();
}

// 测试日志提交
TEST(LogCollectorTest, LogSubmission) {
    CollectorConfig config;
    config.batchSize = 2;  // 设置很小的缓冲区大小以触发自动刷新
    config.flushInterval = std::chrono::milliseconds(100);  // 设置较短的刷新间隔
    
    // 创建日志收集器
    LogCollector collector(config);
    
    // 设置错误回调
    collector.SetErrorCallback([](const std::string& error) {
        std::cerr << "Error: " << error << std::endl;
    });
    
    // 添加日志
    collector.SubmitLog("日志消息1", LogLevel::INFO);
    collector.SubmitLog("日志消息2", LogLevel::WARNING);
    
    // 等待自动刷新
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
}

// 测试批量日志提交
TEST(LogCollectorTest, BatchLogSubmission) {
    CollectorConfig config;
    LogCollector collector(config);
    
    std::vector<std::string> logs = {
        "日志消息1",
        "日志消息2",
        "日志消息3"
    };
    
    collector.SubmitLogs(logs, LogLevel::INFO);
    
    // 手动刷新
    collector.Flush();
} 