#include "xumj/collector/log_collector.h"
#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <random>

using namespace xumj::collector;

// 生成随机日志内容
std::string generateRandomLog(std::mt19937& rng) {
    // 定义可能的日志模板
    const std::vector<std::string> logTemplates = {
        "User {} logged in from IP {}",
        "Database query took {} ms to execute: {}",
        "API request to {} returned status code {}",
        "Memory usage: {}MB, CPU usage: {}%",
        "File {} was accessed by user {}",
        "Service {} started with PID {}",
        "Connection to {} timed out after {} ms",
        "Cache hit ratio: {}%, cache size: {} entries",
        "Job {} completed in {} seconds",
        "Error occurred while processing request: {}"
    };
    
    // 选择一个随机模板
    std::uniform_int_distribution<size_t> templateDist(0, logTemplates.size() - 1);
    std::string logTemplate = logTemplates[templateDist(rng)];
    
    // 替换模板中的占位符
    size_t pos = logTemplate.find("{}");
    while (pos != std::string::npos) {
        std::string replacement;
        
        // 生成不同类型的随机值
        std::uniform_int_distribution<int> typeDist(0, 4);
        int valueType = typeDist(rng);
        
        switch (valueType) {
            case 0: { // 整数
                std::uniform_int_distribution<int> intDist(1, 10000);
                replacement = std::to_string(intDist(rng));
                break;
            }
            case 1: { // 浮点数
                std::uniform_real_distribution<double> floatDist(0.0, 100.0);
                replacement = std::to_string(floatDist(rng)).substr(0, 5);
                break;
            }
            case 2: { // IP地址
                std::uniform_int_distribution<int> octetDist(0, 255);
                replacement = std::to_string(octetDist(rng)) + "." +
                              std::to_string(octetDist(rng)) + "." +
                              std::to_string(octetDist(rng)) + "." +
                              std::to_string(octetDist(rng));
                break;
            }
            case 3: { // 用户名
                const std::vector<std::string> users = {"admin", "user", "guest", "system", "root"};
                std::uniform_int_distribution<size_t> userDist(0, users.size() - 1);
                replacement = users[userDist(rng)];
                break;
            }
            case 4: { // SQL查询
                const std::vector<std::string> queries = {
                    "SELECT * FROM users", 
                    "INSERT INTO logs VALUES(...)", 
                    "UPDATE settings SET value='new'", 
                    "DELETE FROM cache WHERE expired=true"
                };
                std::uniform_int_distribution<size_t> queryDist(0, queries.size() - 1);
                replacement = queries[queryDist(rng)];
                break;
            }
        }
        
        logTemplate.replace(pos, 2, replacement);
        pos = logTemplate.find("{}", pos + replacement.length());
    }
    
    return logTemplate;
}

// 生成随机日志级别
LogLevel generateRandomLogLevel(std::mt19937& rng) {
    // 调整各级别的权重，使得INFO级别出现概率最高
    const std::vector<std::pair<LogLevel, int>> levelWeights = {
        {LogLevel::TRACE, 5},
        {LogLevel::DEBUG, 10},
        {LogLevel::INFO, 65},
        {LogLevel::WARNING, 15},
        {LogLevel::ERROR, 4},
        {LogLevel::CRITICAL, 1}
    };
    
    int totalWeight = 0;
    for (const auto& pair : levelWeights) {
        totalWeight += pair.second;
    }
    
    std::uniform_int_distribution<int> dist(1, totalWeight);
    int roll = dist(rng);
    
    int cumulativeWeight = 0;
    for (const auto& pair : levelWeights) {
        cumulativeWeight += pair.second;
        if (roll <= cumulativeWeight) {
            return pair.first;
        }
    }
    
    return LogLevel::INFO; // 默认返回INFO级别
}

int main() {
    // 创建并配置日志收集器
    CollectorConfig config;
    config.collectorId = "example-collector";
    config.serverAddress = "127.0.0.1";  // 在实际应用中，这里应该是真实的服务器地址
    config.serverPort = 8080;
    config.batchSize = 10;  // 为了示例，使用较小的批处理大小
    config.flushInterval = std::chrono::milliseconds(1000);
    config.threadPoolSize = 2;
    config.minLevel = LogLevel::INFO;  // 只收集INFO及以上级别的日志
    
    // 创建日志收集器
    LogCollector collector;
    
    // 设置回调函数
    collector.SetSendCallback([](size_t count) {
        std::cout << "成功发送了 " << count << " 条日志" << std::endl;
    });
    
    collector.SetErrorCallback([](const std::string& error) {
        std::cerr << "错误: " << error << std::endl;
    });
    
    // 初始化收集器
    if (!collector.Initialize(config)) {
        std::cerr << "无法初始化日志收集器" << std::endl;
        return 1;
    }
    
    // 添加一个关键字过滤器，过滤掉包含"error"的日志
    collector.AddFilter(std::make_shared<KeywordFilter>(
        std::vector<std::string>{"error"}, true));
    
    std::cout << "日志收集器示例启动，按Ctrl+C停止..." << std::endl;
    
    // 创建随机数生成器
    std::random_device rd;
    std::mt19937 rng(rd());
    
    // 模拟生成日志
    int logCounter = 0;
    while (true) {
        // 生成随机日志
        std::string logContent = generateRandomLog(rng);
        LogLevel level = generateRandomLogLevel(rng);
        
        // 提交日志
        collector.SubmitLog(logContent, level);
        
        // 每隔一段时间生成一批日志
        if (++logCounter % 5 == 0) {
            std::vector<std::string> batchLogs;
            for (int i = 0; i < 5; ++i) {
                batchLogs.push_back(generateRandomLog(rng));
            }
            collector.SubmitLogs(batchLogs, LogLevel::INFO);
        }
        
        // 模拟产生日志的频率
        std::uniform_int_distribution<int> sleepDist(100, 500);
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepDist(rng)));
        
        // 为了防止示例无限运行，这里限制运行10秒钟
        if (logCounter > 100) {
            break;
        }
    }
    
    // 强制刷新剩余日志
    collector.Flush();
    
    // 等待一段时间确保所有日志都被处理
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 关闭收集器
    collector.Shutdown();
    
    std::cout << "日志收集器示例完成" << std::endl;
    
    return 0;
} 