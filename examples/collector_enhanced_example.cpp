/**
 * @file collector_enhanced_example.cpp
 * @brief 增强版日志收集器示例程序，用于全面测试 LogCollector 功能
 */

#include <xumj/collector/log_collector.h>
#include <xumj/common/memory_pool.h>
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <atomic>
#include <mutex>
#include <iomanip>
#include <ctime>
#include <sstream>
#include <functional>
#include <cstring>

using namespace xumj;
using namespace xumj::collector;

// 日志级别转字符串
std::string LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// 统计信息结构
struct TestStats {
    std::atomic<int> totalSubmitted{0};
    std::atomic<int> totalSent{0};
    std::atomic<int> totalFiltered{0};
    std::atomic<int> errors{0};
    
    void print() const {
        std::cout << "测试统计信息：" << std::endl;
        std::cout << "  总提交日志数: " << totalSubmitted << std::endl;
        std::cout << "  成功发送日志数: " << totalSent << std::endl;
        std::cout << "  被过滤日志数: " << totalFiltered << std::endl;
        std::cout << "  错误数: " << errors << std::endl;
        
        double successRate = (totalSubmitted > 0) ? 
                            (static_cast<double>(totalSent) / totalSubmitted * 100.0) : 0.0;
        std::cout << "  成功率: " << std::fixed << std::setprecision(2) << successRate << "%" << std::endl;
    }
};

// 全局统计
TestStats g_stats;

// 生成随机日志内容
std::string generateRandomContent(size_t length = 100) {
    static const char charset[] = 
        "0123456789"
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        " ,.;:!?()-_+=[]{}|<>/\\\"'`~@#$%^&*";
    
    static std::mt19937 generator(std::random_device{}());
    static std::uniform_int_distribution<int> distribution(0, sizeof(charset) - 2);
    
    std::string result;
    result.reserve(length);
    
    for (size_t i = 0; i < length; ++i) {
        result += charset[distribution(generator)];
    }
    
    return result;
}

// 生成随机日志级别，遵循真实分布 (INFO多，FATAL少)
LogLevel generateRandomLevel() {
    static std::mt19937 generator(std::random_device{}());
    static std::discrete_distribution<int> distribution({5, 15, 75, 15, 4, 1}); 
    // TRACE=5%, DEBUG=15%, INFO=75%, WARNING=15%, ERROR=4%, CRITICAL=1%
    
    int level = distribution(generator);
    return static_cast<LogLevel>(level);
}

// 获取当前时间字符串
std::string getCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    auto time = std::chrono::system_clock::to_time_t(now);
    
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// 打印测试标题
void printTestHeader(const std::string& title) {
    std::cout << "\n==========================================" << std::endl;
    std::cout << "  " << title << std::endl;
    std::cout << "==========================================" << std::endl;
}

// 创建自定义过滤器
class CustomLogFilter : public LogFilterInterface {
public:
    CustomLogFilter(std::function<bool(LogLevel, const std::string&)> filterFunc)
        : filterFunc_(std::move(filterFunc)) {}
        
    bool ShouldFilter(const LogEntry& entry) const override {
        return filterFunc_(entry.GetLevel(), entry.GetContent());
    }
    
private:
    std::function<bool(LogLevel, const std::string&)> filterFunc_;
};

// 基本功能测试
//主要测试SubmitLog和SubmitLogs的功能
void runBasicTest(LogCollector& collector) {
    printTestHeader("基本功能测试");
    
    // 测试单条日志提交
    std::cout << "提交单条日志..." << std::endl;
    for (int i = 0; i < 50; ++i) {
        LogLevel level = generateRandomLevel();
        std::string content = "测试日志 #" + std::to_string(i+1) + ": " + generateRandomContent(50);
        
        bool success = collector.SubmitLog(content, level);
        std::cout << "  日志 #" << (i+1) << " [" << LogLevelToString(level) << "]: " 
                  << (success ? "提交成功" : "提交失败") << std::endl;
        
        // // 每条日志都强制刷新，确保所有日志都能被立即发送并显示
        // std::cout << "  刷新日志 #" << (i+1) << "..." << std::endl;
        // collector.Flush();
        // 添加短暂延时确保日志输出完成
        //std::this_thread::sleep_for(std::chrono::milliseconds(100));

        g_stats.totalSubmitted++;
        if (success) {
            g_stats.totalSent++;
        } else {
            g_stats.errors++;
        }
        
        // 短暂停顿
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    std::cout << "单条日志提交完成，总共发送50条日志" << std::endl;

    // 测试批量日志提交
    std::cout << "提交批量日志..." << std::endl;
    std::vector<std::string> logs;
    for (int i = 0; i < 50; ++i) {
        std::string content = "批量日志 #" + std::to_string(i+1) + ": " + generateRandomContent(50);
        logs.push_back(content);
    }
    
    bool success = collector.SubmitLogs(logs, LogLevel::INFO);
    std::cout << "  批量提交结果: " << (success ? "成功" : "失败") << std::endl;
    
    g_stats.totalSubmitted += logs.size();
    if (success) {
        g_stats.totalSent += logs.size();
    } else {
        g_stats.errors += logs.size();
    }
    
    std::cout << "基本功能测试完成" << std::endl;
}

// 过滤器测试
void runFilterTest(LogCollector& collector) {
    printTestHeader("过滤器测试");
    
    // 添加级别过滤器 - 仅接受 INFO 级别及以上
    auto levelFilter = std::make_shared<LevelFilter>(LogLevel::INFO);
    collector.AddFilter(levelFilter);
    std::cout << "添加了级别过滤器 (INFO及以上)" << std::endl;
    
    // 添加关键字过滤器 - 拒绝包含"error"关键字的日志
    auto keywordFilter = std::make_shared<KeywordFilter>(std::vector<std::string>{"error"}, true);
    collector.AddFilter(keywordFilter);
    std::cout << "添加了内容过滤器 (拒绝包含'error')" << std::endl;
    
    // 测试过滤效果
    std::vector<std::pair<LogLevel, std::string>> testLogs = {
        {LogLevel::TRACE, "这是一条TRACE日志"},
        {LogLevel::INFO, "这是一条普通INFO日志"},
        {LogLevel::WARNING, "这是一条警告日志"},
        {LogLevel::ERROR, "这是一条错误日志"},
        {LogLevel::INFO, "这是一条包含error关键字的INFO日志"}
    };
    
    std::cout << "测试日志过滤..." << std::endl;
    for (const auto& log : testLogs) {
        bool filtered = false;
        // 创建LogEntry对象来测试过滤器
        LogEntry entry(log.second, log.first);
        // 检查是否被过滤
        if (levelFilter->ShouldFilter(entry) || keywordFilter->ShouldFilter(entry)) {
            filtered = true;
        }
        
        bool success = !filtered && collector.SubmitLog(log.second, log.first);
        std::cout << "  [" << LogLevelToString(log.first) << "] " << log.second.substr(0, 30)
                  << "... : " << (success ? "通过" : "被过滤") << std::endl;
        
        g_stats.totalSubmitted++;
        if (success) {
            g_stats.totalSent++;
        } else {
            g_stats.totalFiltered++;
        }
    }
    
    // 清除所有过滤器
    collector.ClearFilters();
    std::cout << "清除了所有过滤器" << std::endl;
    
    std::cout << "过滤器测试完成" << std::endl;
}

// 多线程并发测试
void runConcurrencyTest(LogCollector& collector) {
    printTestHeader("多线程并发测试");
    
    std::mutex thread_mutex_;

    const int numThreads = 8;
    const int logsPerThread = 100;
    
    std::cout << "启动 " << numThreads << " 个线程，每个线程提交 " << logsPerThread << " 条日志" << std::endl;
    
    std::vector<std::thread> threads;
    std::atomic<int> threadsCompleted{0};
    
    for (int t = 0; t < numThreads; ++t) {
        threads.emplace_back([&collector, t, logsPerThread, &threadsCompleted, &thread_mutex_]() {
            for (int i = 0; i < logsPerThread; ++i) {
                LogLevel level = generateRandomLevel();
                std::string content = "Thread-" + std::to_string(t) + 
                                     " Log-" + std::to_string(i) + ": " + 
                                     generateRandomContent(30);
                
                bool success = collector.SubmitLog(content, level);
                
                g_stats.totalSubmitted++;
                if (success) {
                    g_stats.totalSent++;
                } else {
                    g_stats.errors++;
                }
                
                // 随机延迟模拟真实场景
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
            }
            
            threadsCompleted++;
            std::lock_guard<std::mutex> lock(thread_mutex_);
            std::cout << "  完成: " << threadsCompleted << "/" << numThreads << " 线程\r" << std::flush;
        });
    }
    
    
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }
    std::cout << "\n所有线程已完成" << std::endl;
    std::cout << "多线程并发测试完成" << std::endl;
}

// 内存池性能测试
// 功能还没开发好
void runMemoryPoolTest(LogCollector& collector) {
    printTestHeader("内存池性能测试");
    
    const int numLogs = 1000;
    
    std::cout << "提交 " << numLogs << " 条日志测试内存池性能..." << std::endl;
    
    // 使用内存池的时间
    auto startWithPool = std::chrono::high_resolution_clock::now();
    
    for (int i = 0; i < numLogs; ++i) {
        LogLevel level = generateRandomLevel();
        std::string content = "MemoryPool-Test-" + std::to_string(i) + ": " + 
                              generateRandomContent(200); // 较大的日志
        
        bool success = collector.SubmitLog(content, level);
        
        g_stats.totalSubmitted++;
        if (success) {
            g_stats.totalSent++;
        } else {
            g_stats.errors++;
        }
    }
    
    auto endWithPool = std::chrono::high_resolution_clock::now();
    auto durationWithPool = std::chrono::duration_cast<std::chrono::milliseconds>(
        endWithPool - startWithPool).count();
    
    std::cout << "内存池处理 " << numLogs << " 条日志用时: " << durationWithPool << " ms" << std::endl;
    std::cout << "平均每条日志: " << (static_cast<double>(durationWithPool) / numLogs) << " ms" << std::endl;
    
    std::cout << "内存池性能测试完成" << std::endl;
}

// 压力测试
void runStressTest(LogCollector& collector) {
    printTestHeader("压力测试");
    
    const int numLogs = 10000;
    
    std::cout << "一次性提交 " << numLogs << " 条日志..." << std::endl;
    
    auto start = std::chrono::high_resolution_clock::now();
    
    // 快速生成大量日志
    std::vector<std::string> logs;
    logs.reserve(numLogs);
    
    for (int i = 0; i < numLogs; ++i) {
        std::string content = "Stress-Test-" + std::to_string(i) + ": " + 
                              generateRandomContent(50);
        logs.push_back(content);
    }
    
    // 分批提交
    const int batchSize = 1000;
    int totalSuccess = 0;
    
    for (int i = 0; i < numLogs; i += batchSize) {
        int currentBatchSize = std::min(batchSize, numLogs - i);
        std::vector<std::string> batch(
            logs.begin() + i, 
            logs.begin() + i + currentBatchSize
        );
        
        bool success = collector.SubmitLogs(batch, LogLevel::INFO);
        if (success) {
            totalSuccess += currentBatchSize;
        }
        
        // 显示进度
        std::cout << "  进度: " << (i + currentBatchSize) << "/" << numLogs 
                  << " (" << (i + currentBatchSize) * 100 / numLogs << "%)\r" << std::flush;
    }
    
    auto end = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
    
    std::cout << "\n压力测试完成: " << totalSuccess << "/" << numLogs << " 成功" << std::endl;
    std::cout << "总用时: " << duration << " ms" << std::endl;
    std::cout << "平均速率: " << (static_cast<double>(numLogs) / duration * 1000) << " 日志/秒" << std::endl;
    
    g_stats.totalSubmitted += numLogs;
    g_stats.totalSent += totalSuccess;
    g_stats.errors += (numLogs - totalSuccess);
}

// 错误恢复测试
void runRecoveryTest(LogCollector& collector) {
    printTestHeader("错误恢复测试");
    
    // 模拟服务器连接断开 - 创建一个新的配置并初始化
    std::cout << "模拟服务器连接断开..." << std::endl;
    CollectorConfig badConfig;
    badConfig.serverAddress = "non-existent-server";
    badConfig.serverPort = 1234;
    badConfig.batchSize = 50;
    badConfig.flushInterval = std::chrono::milliseconds(1000);
    badConfig.maxQueueSize = 10000;
    badConfig.threadPoolSize = 4;
    badConfig.memoryPoolSize = 4096;
    badConfig.minLevel = LogLevel::INFO;
    badConfig.compressLogs = true;
    badConfig.enableRetry = true;
    badConfig.maxRetryCount = 3;
    badConfig.retryInterval = std::chrono::milliseconds(1000);
    
    collector.Initialize(badConfig);
    
    // 在连接断开情况下提交日志
    const int numLogs = 50;
    std::cout << "在断开连接状态提交 " << numLogs << " 条日志..." << std::endl;
    
    for (int i = 0; i < numLogs; ++i) {
        LogLevel level = generateRandomLevel();
        std::string content = "Recovery-Test-" + std::to_string(i) + ": " + 
                             generateRandomContent(50);
        
        bool success = collector.SubmitLog(content, level);
        
        g_stats.totalSubmitted++;
        if (success) {
            g_stats.totalSent++;
        } else {
            g_stats.errors++;
        }
        
        if (i % 10 == 0) {
            std::cout << "  已提交: " << i << "/" << numLogs << std::endl;
        }
    }
    
    // 模拟恢复连接 - 回到正常配置
    std::cout << "恢复服务器连接..." << std::endl;
    CollectorConfig goodConfig;
    goodConfig.serverAddress = "localhost";
    goodConfig.serverPort = 9000;
    goodConfig.batchSize = 10;
    goodConfig.flushInterval = std::chrono::milliseconds(1000);
    goodConfig.maxQueueSize = 10000;
    goodConfig.threadPoolSize = 4;
    goodConfig.memoryPoolSize = 4096;
    goodConfig.minLevel = LogLevel::INFO;
    goodConfig.compressLogs = true;
    goodConfig.enableRetry = true;
    goodConfig.maxRetryCount = 3;
    goodConfig.retryInterval = std::chrono::milliseconds(1000);
    
    collector.Initialize(goodConfig);
    
    // 等待重试逻辑生效
    std::cout << "等待重试机制生效..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    std::cout << "错误恢复测试完成" << std::endl;
}

int main() {
    std::cout << "**********************************************" << std::endl;
    std::cout << "*       增强版日志收集器测试程序 v1.0        *" << std::endl;
    std::cout << "**********************************************" << std::endl;
    std::cout << "当前时间: " << getCurrentTimeStr() << std::endl;
    
    // 配置日志收集器
    CollectorConfig config;
    config.collectorId = "enhanced-test-collector";
    config.serverAddress = "localhost";       // 日志服务器地址
    config.serverPort = 9000;                // 日志服务器端口
    config.maxRetryCount = 3;                // 最大重试次数
    config.retryInterval = std::chrono::milliseconds(1000);  // 重试间隔(毫秒)
    config.batchSize = 50;                   // 批量发送大小
    config.flushInterval = std::chrono::milliseconds(500);  // 自动刷新间隔(毫秒)
    config.compressLogs = true;              // 启用压缩
    config.maxQueueSize = 10000;             // 最大队列大小
    config.threadPoolSize = 4;               // 工作线程数
    config.memoryPoolSize = 4096;            // 内存池大小
    config.minLevel = LogLevel::INFO;        // 最低日志级别
    
    std::cout << "\n配置信息：" << std::endl;
    std::cout << "  服务器: " << config.serverAddress << ":" << config.serverPort << std::endl;
    std::cout << "  批量大小: " << config.batchSize << std::endl;
    std::cout << "  最低日志级别: " << LogLevelToString(config.minLevel) << std::endl;
    std::cout << "  压缩: " << (config.compressLogs ? "启用" : "禁用") << std::endl;
    std::cout << "  工作线程: " << config.threadPoolSize << std::endl;
    std::cout << "  内存池大小: " << config.memoryPoolSize << std::endl;
    
    // 初始化日志收集器
    LogCollector collector;
    collector.Initialize(config);
    
    try {
        // 设置回调函数
        collector.SetSendCallback([](size_t count) {
            std::cout << "成功发送 " << count << " 条日志" << std::endl;
        });
        
        collector.SetErrorCallback([](const std::string& error) {
            std::cout << "发送错误: " << error << std::endl;
        });
        
        // 运行各种测试
         runBasicTest(collector); 
        // runFilterTest(collector);
        // runMemoryPoolTest(collector);//功能每开发好，这个先忽略
        // runConcurrencyTest(collector);
        // runRecoveryTest(collector);
        // runStressTest(collector);
        
        // 确保所有日志都被发送
        std::cout << "\n刷新所有待发送日志..." << std::endl;
        collector.Flush();
        
        // 等待最后的处理完成
        std::this_thread::sleep_for(std::chrono::seconds(3));
        
        // 打印最终统计
        std::cout << "\n============== 最终测试结果 ==============" << std::endl;
        g_stats.print();
        
        std::cout << "\n测试完成！" << std::endl;
        std::cout << "当前时间: " << getCurrentTimeStr() << std::endl;
        
        // 关闭收集器
        collector.Shutdown();
        
    } catch (const std::exception& e) {
        std::cerr << "测试过程中发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 