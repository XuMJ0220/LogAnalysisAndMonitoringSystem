#include "xumj/collector/log_collector.h"
#include <iostream>
#include <thread>
#include <chrono>

using namespace xumj::collector;

int main() {
    // 配置日志收集器
    CollectorConfig config;
    config.collectorId = "file-watcher-collector";
    config.serverAddress = "127.0.0.1";
    config.serverPort = 8080;
    config.batchSize = 10;
    config.flushInterval = std::chrono::milliseconds(1000);
    config.threadPoolSize = 2;
    config.minLevel = LogLevel::INFO;

    LogCollector collector;
    collector.SetSendCallback([](size_t count) {
        std::cout << "已发送 " << count << " 条日志" << std::endl;
    });
    collector.SetErrorCallback([](const std::string& error) {
        std::cerr << "错误: " << error << std::endl;
    });

    if (!collector.Initialize(config)) {
        std::cerr << "无法初始化日志收集器" << std::endl;
        return 1;
    }

    // 自动采集logs/test_service.log文件内容
    collector.CollectFromFile("/home/xumj/项目/Distributed-Real-time-Log-Analysis-and-Monitoring-System/logs/test_service.log", LogLevel::INFO, 1000, 10);

    std::cout << "日志收集器已启动，正在实时采集../logs/test_service.log，按Ctrl+C退出..." << std::endl;

    // 主线程保持运行
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }

    collector.Shutdown();
    return 0;
} 