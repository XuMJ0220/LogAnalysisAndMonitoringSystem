#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <vector>
#include <csignal>

#include "xumj/collector/log_collector.h"
#include "xumj/processor/log_processor.h"
#include "xumj/analyzer/log_analyzer.h"
#include "xumj/storage/storage_factory.h"
#include "xumj/storage/redis_storage.h"
#include "xumj/storage/mysql_storage.h"
#include "xumj/alert/alert_manager.h"
#include "xumj/network/tcp_server.h"
#include "xumj/network/tcp_client.h"

using namespace xumj::collector;
using namespace xumj::processor;
using namespace xumj::analyzer;
using namespace xumj::storage;
using namespace xumj::alert;
using namespace xumj::network;

std::atomic<bool> running(true);

// 信号处理函数，用于优雅地退出
void signalHandler(int signum) {
    std::cout << "收到信号 " << signum << "，准备退出..." << std::endl;
    running = false;
}

// 辅助函数：获取当前时间字符串
std::string getCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = *std::localtime(&time_t_now);
    char buffer[30];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_now);
    return std::string(buffer);
}

// 创建并配置日志收集器
std::shared_ptr<LogCollector> setupLogCollector() {
    // 创建日志收集器配置
    CollectorConfig collectorConfig;
    collectorConfig.maxQueueSize = 1000;         // 日志缓冲区大小
    collectorConfig.flushInterval = std::chrono::milliseconds(2000);         // 刷新间隔，毫秒
    collectorConfig.compressLogs = true;    // 启用压缩
    collectorConfig.maxRetryCount = 3;               // 重试次数
    collectorConfig.retryInterval = std::chrono::milliseconds(1000);         // 重试间隔，毫秒
    collectorConfig.minLevel = LogLevel::DEBUG;        // 最低日志级别
    
    // 初始化日志收集器
    auto collector = std::make_shared<LogCollector>(collectorConfig);
    
    // 注册一个简单的过滤器，过滤掉包含"DEBUG"的日志
    collector->AddFilter(std::make_shared<LevelFilter>(LogLevel::DEBUG));
    
    std::cout << "日志收集器配置完成" << std::endl;
    return collector;
}

// 创建并配置日志处理器
std::shared_ptr<LogProcessor> setupLogProcessor() {
    // 创建日志处理器配置
    // 这里假设LogProcessor支持默认构造
    auto processor = std::make_shared<LogProcessor>();
    
    // 添加日志解析器（后续补充stub）
    // processor->AddParser("JSON", std::make_shared<JsonLogParser>());
    // processor->AddParser("Syslog", std::make_shared<SyslogParser>());
    // processor->AddParser("Apache", std::make_shared<ApacheLogParser>());
    
    std::cout << "日志处理器配置完成" << std::endl;
    return processor;
}

// 创建并配置日志分析器
std::shared_ptr<LogAnalyzer> setupLogAnalyzer() {
    // 这里假设LogAnalyzer支持默认构造
    auto analyzer = std::make_shared<LogAnalyzer>();
    // 添加分析规则（后续补充stub）
    // analyzer->AddRule("SystemResource", std::make_shared<SystemResourceRule>());
    // analyzer->AddRule("Performance", std::make_shared<PerformanceRule>());
    // analyzer->AddRule("UserLogin", std::make_shared<UserLoginRule>());
    // analyzer->AddRule("ErrorPattern", std::make_shared<ErrorPatternRule>());
    std::cout << "日志分析器配置完成" << std::endl;
    return analyzer;
}

// 创建并配置存储管理器
std::shared_ptr<StorageFactory> setupStorage() {
    // 创建Redis存储配置
    RedisConfig redisConfig;
    redisConfig.host = "127.0.0.1";
    redisConfig.port = 6379;
    redisConfig.password = "";
    redisConfig.database = 0;
    // 创建MySQL存储配置
    MySQLConfig mysqlConfig;
    mysqlConfig.host = "127.0.0.1";
    mysqlConfig.port = 3306;
    mysqlConfig.username = "loguser";
    mysqlConfig.password = "logpassword";
    mysqlConfig.database = "log_analysis";
    // 初始化存储工厂
    auto storageFactory = std::make_shared<StorageFactory>();
    // 注册存储实现（如无RegisterStorage方法可注释）
    // storageFactory->RegisterStorage("redis", std::make_shared<RedisStorage>(redisConfig));
    // storageFactory->RegisterStorage("mysql", std::make_shared<MySQLStorage>(mysqlConfig));
    std::cout << "存储管理器配置完成" << std::endl;
    return storageFactory;
}

// 创建并配置告警管理器
std::shared_ptr<AlertManager> setupAlertManager() {
    // 这里假设AlertManager支持默认构造
    auto alertManager = std::make_shared<AlertManager>();
    std::cout << "告警管理器配置完成" << std::endl;
    return alertManager;
}

// 创建并配置TCP服务器
std::shared_ptr<TcpServer> setupServer(
    std::shared_ptr<LogProcessor> processor,
    std::shared_ptr<LogAnalyzer> analyzer,
    std::shared_ptr<AlertManager> alertManager) {

    // 创建TCP服务器
    auto server = std::make_shared<TcpServer>("LogServer", "0.0.0.0", 8000, 4);
    
    // 设置连接回调
    server->SetConnectionCallback([](uint64_t connectionId, const std::string& clientAddr, bool connected) {
        if (connected) {
            std::cout << "新客户端连接: " << clientAddr << ", ID: " << connectionId << std::endl;
        } else {
            std::cout << "客户端断开连接: " << clientAddr << ", ID: " << connectionId << std::endl;
        }
    });
    
    // 设置消息回调，将接收的日志数据放入处理器
    server->SetMessageCallback([processor, analyzer, alertManager](
        uint64_t connectionId, 
        const std::string& message,
        muduo::Timestamp timestamp) {
        
        std::cout << "接收到消息，长度: " << message.size() << " 字节" << std::endl;
        
        // 创建日志数据
        LogData logData;
        logData.id = "log-" + std::to_string(connectionId) + "-" + std::to_string(timestamp.microSecondsSinceEpoch());
        logData.data = message;
        logData.source = "client-" + std::to_string(connectionId);
        logData.timestamp = std::chrono::system_clock::now();
        
        // 处理日志
        processor->SubmitLogData(logData);
    });
    
    std::cout << "TCP服务器配置完成" << std::endl;
    return server;
}

// 示例客户端，用于生成和发送日志
void runClient() {
    // 创建TCP客户端
    TcpClient client("LogClient", "127.0.0.1", 8000, true);
    
    // 设置连接回调
    client.SetConnectionCallback([](bool connected) {
        if (connected) {
            std::cout << "已连接到服务器" << std::endl;
        } else {
            std::cout << "与服务器断开连接" << std::endl;
        }
    });
    
    // 设置消息回调
    client.SetMessageCallback([](const std::string& message, muduo::Timestamp) {
        std::cout << "收到服务器响应: " << message;
    });
    
    // 连接服务器
    client.Connect();
    
    // 等待连接建立
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 生成并发送示例日志
    int index = 0;
    while (running && index < 20) {
        // 生成不同类型的示例日志
        std::string logMessage;
        
        if (index % 5 == 0) {
            // 系统资源日志
            logMessage = "{\"type\": \"system\", \"timestamp\": \"" + 
                getCurrentTimeStr() + "\", \"level\": \"WARNING\", " +
                "\"cpu_usage\": " + std::to_string(60 + index % 40) + ", " +
                "\"memory_usage\": " + std::to_string(70 + index % 30) + ", " +
                "\"disk_usage\": " + std::to_string(50 + index % 50) + ", " +
                "\"message\": \"系统资源使用监控\", " +
                "\"server\": \"server" + std::to_string(1 + index % 5) + "\"}";
                
        } else if (index % 5 == 1) {
            // 性能日志
            logMessage = "{\"type\": \"performance\", \"timestamp\": \"" + 
                getCurrentTimeStr() + "\", \"level\": \"INFO\", " +
                "\"query_time\": " + std::to_string(200 + index * 20) + ", " +
                "\"query_id\": \"Q" + std::to_string(index) + "\", " +
                "\"rows_examined\": " + std::to_string(100 * index) + ", " +
                "\"message\": \"数据库查询性能\", " +
                "\"database\": \"users\"}";
                
        } else if (index % 5 == 2) {
            // 用户登录日志
            bool success = (index % 3 != 0);
            logMessage = "{\"type\": \"user_activity\", \"timestamp\": \"" + 
                getCurrentTimeStr() + "\", \"level\": \"" + (success ? "INFO" : "ERROR") + "\", " +
                "\"user_id\": \"user" + std::to_string(index) + "\", " +
                "\"action\": \"login\", " +
                "\"status\": \"" + (success ? "success" : "failed") + "\", " +
                "\"reason\": \"" + (success ? "正常登录" : "密码错误") + "\", " +
                "\"ip_address\": \"192.168.1." + std::to_string(index % 256) + "\", " +
                "\"message\": \"用户" + (success ? "登录成功" : "登录失败") + "\"}";
                
        } else if (index % 5 == 3) {
            // 错误日志
            logMessage = "{\"type\": \"error\", \"timestamp\": \"" + 
                getCurrentTimeStr() + "\", \"level\": \"ERROR\", " +
                "\"error_code\": \"E" + std::to_string(1000 + index) + "\", " +
                "\"component\": \"payment_service\", " +
                "\"message\": \"支付处理失败: 超时等待第三方响应\", " +
                "\"transaction_id\": \"T" + std::to_string(index) + "\"}";
                
        } else {
            // 应用日志
            logMessage = "{\"type\": \"application\", \"timestamp\": \"" + 
                getCurrentTimeStr() + "\", \"level\": \"INFO\", " +
                "\"module\": \"cart_service\", " +
                "\"session_id\": \"session-" + std::to_string(1000 + index) + "\", " +
                "\"message\": \"用户添加商品到购物车\", " +
                "\"user_id\": \"user" + std::to_string(index) + "\", " +
                "\"product_id\": \"product" + std::to_string(index * 5) + "\"}";
        }
        
        // 发送日志
        std::cout << "发送日志 #" << index << ": " << logMessage.substr(0, 50) << "..." << std::endl;
        client.Send(logMessage);
        
        // 等待一段时间
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
        index++;
    }
    
    // 断开连接
    client.Disconnect();
    std::cout << "客户端已断开连接" << std::endl;
}

int main() {
    std::cout << "启动分布式实时日志分析与监控系统示例..." << std::endl;
    
    // 注册信号处理器
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    try {
        // 初始化各个组件
        auto logCollector = setupLogCollector();
        auto logProcessor = setupLogProcessor();
        auto logAnalyzer = setupLogAnalyzer();
        auto storageFactory = setupStorage();
        auto alertManager = setupAlertManager();
        
        // 启动告警管理器
        if (!alertManager->Start()) {
            std::cerr << "启动告警管理器失败" << std::endl;
            return 1;
        }
        
        // 设置并启动TCP服务器
        auto server = setupServer(logProcessor, logAnalyzer, alertManager);
        
        server->Start();
        std::cout << "TCP服务器已启动，监听端口: 8000" << std::endl;
        
        // 启动客户端线程
        std::thread clientThread(runClient);
        
        // 主循环
        std::cout << "系统已启动，按Ctrl+C停止..." << std::endl;
        while (running) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        // 等待客户端线程结束
        if (clientThread.joinable()) {
            clientThread.join();
        }
        
        // 停止服务器
        server->Stop();
        std::cout << "TCP服务器已停止" << std::endl;
        
        // 停止告警管理器
        alertManager->Stop();
        std::cout << "告警管理器已停止" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "错误: " << e.what() << std::endl;
        return 1;
    }
    
    std::cout << "系统已正常退出" << std::endl;
    return 0;
} 