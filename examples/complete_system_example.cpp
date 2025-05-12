#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <vector>
#include <csignal>
#include <unordered_map>
#include <future>
#include <sstream>

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
    LogProcessorConfig config;
    config.workerThreads = 4;
    config.queueSize = 1000;
    config.enableRedisStorage = true;
    config.enableMySQLStorage = true;
    config.debug = true;  // 启用调试模式
    
    // 创建处理器实例
    auto processor = std::make_shared<LogProcessor>(config);
    
    // 添加JSON日志解析器
    processor->AddLogParser(std::make_shared<JsonLogParser>());
    
    // 启动处理器 - 这会自动初始化TCP服务器
    bool success = processor->Start();
    if (!success) {
        std::cerr << "启动日志处理器失败" << std::endl;
        return nullptr;
    }
    
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
    redisConfig.password = "123465";  // 使用我们设置的密码
    redisConfig.database = 0;
    redisConfig.timeout = 10000;  // 增加超时时间到10秒

    // 创建MySQL存储配置
    MySQLConfig mysqlConfig;
    mysqlConfig.host = "127.0.0.1";
    mysqlConfig.port = 3306;
    mysqlConfig.username = "root";  // 修改 user 为 username
    mysqlConfig.password = "ytfhqqkso1";
    mysqlConfig.database = "log_analysis";

    // 初始化存储工厂
    auto storageFactory = std::make_shared<StorageFactory>();
    
    // 尝试注册Redis存储实现
    try {
        std::cout << "正在连接Redis服务器: " << redisConfig.host << ":" << redisConfig.port << "..." << std::endl;
        storageFactory->RegisterStorage("redis", std::make_shared<RedisStorage>(redisConfig));
        std::cout << "Redis连接成功!" << std::endl;
    } catch (const RedisStorageException& e) {
        std::cerr << "Redis连接错误: " << e.what() << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Redis连接时发生未知错误: " << e.what() << std::endl;
    }
    
    // 尝试注册MySQL存储实现
    try {
        std::cout << "正在连接MySQL服务器: " << mysqlConfig.host << ":" << mysqlConfig.port << "..." << std::endl;
        storageFactory->RegisterStorage("mysql", std::make_shared<MySQLStorage>(mysqlConfig));
        std::cout << "MySQL连接成功!" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "MySQL连接错误: " << e.what() << std::endl;
    }
    
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
void setupServer(
    std::shared_ptr<LogProcessor> processor,
    std::shared_ptr<LogAnalyzer> /* analyzer */,
    std::shared_ptr<AlertManager> /* alertManager */) {

    // 我们不再需要获取TcpServer实例，因为它已经在LogProcessor内部处理
    std::cout << "TCP服务器已在LogProcessor中初始化，端口：" << processor->GetConfig().tcpPort << std::endl;
}

// 示例客户端，用于生成和发送日志
void runClient(std::shared_ptr<LogProcessor> processor) {
    // 创建TCP客户端
    TcpClient client("LogClient", "127.0.0.1", 8001, true);
    
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
    
    // 等待连接建立，但不要等待过长时间
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 检查是否已连接，如果未连接则不发送日志
    if (!client.IsConnected()) {
        std::cerr << "无法连接到服务器，客户端退出" << std::endl;
        return;
    }
    
    // 生成并发送示例日志
    int index = 0;
    auto startTime = std::chrono::steady_clock::now();
    
    while (running && index < 20) {
        // 检查是否超时（10秒内未完成则退出）
        auto currentTime = std::chrono::steady_clock::now();
        if (std::chrono::duration_cast<std::chrono::seconds>(currentTime - startTime).count() > 10) {
            std::cerr << "客户端发送日志超时，已发送 " << index << " 条" << std::endl;
            break;
        }
        
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
                "\"source\": \"resource_monitor\", " +
                "\"server\": \"server" + std::to_string(1 + index % 5) + "\"}";
                
        } else if (index % 5 == 1) {
            // 性能日志
            logMessage = "{\"type\": \"performance\", \"timestamp\": \"" + 
                getCurrentTimeStr() + "\", \"level\": \"INFO\", " +
                "\"query_time\": " + std::to_string(200 + index * 20) + ", " +
                "\"query_id\": \"Q" + std::to_string(index) + "\", " +
                "\"rows_examined\": " + std::to_string(100 * index) + ", " +
                "\"message\": \"数据库查询性能\", " +
                "\"source\": \"database\", " +
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
                "\"source\": \"auth_service\", " +
                "\"ip_address\": \"192.168.1." + std::to_string(index % 256) + "\", " +
                "\"message\": \"用户" + (success ? "登录成功" : "登录失败") + "\"}";
                
        } else if (index % 5 == 3) {
            // 错误日志
            logMessage = "{\"type\": \"error\", \"timestamp\": \"" + 
                getCurrentTimeStr() + "\", \"level\": \"ERROR\", " +
                "\"error_code\": \"E" + std::to_string(1000 + index) + "\", " +
                "\"component\": \"payment_service\", " +
                "\"source\": \"payment_service\", " +
                "\"message\": \"支付处理失败: 超时等待第三方响应\", " +
                "\"transaction_id\": \"T" + std::to_string(index) + "\"}";
                
        } else {
            // 应用日志
            logMessage = "{\"type\": \"application\", \"timestamp\": \"" + 
                getCurrentTimeStr() + "\", \"level\": \"INFO\", " +
                "\"module\": \"cart_service\", " +
                "\"session_id\": \"session-" + std::to_string(1000 + index) + "\", " +
                "\"message\": \"用户添加商品到购物车\", " +
                "\"source\": \"cart_service\", " +
                "\"user_id\": \"user" + std::to_string(index) + "\", " +
                "\"product_id\": \"product" + std::to_string(index * 5) + "\"}";
        }
        
        // 发送日志
        std::cout << "发送日志 #" << index << ": " << logMessage.substr(0, 50) << "..." << std::endl;
        client.Send(logMessage + "\r\n");  // 添加换行符，确保消息能被正确识别
        
        // 也直接处理这个JSON字符串
        if (processor) {
            bool success = processor->ProcessJsonString(logMessage);
            if (success) {
                std::cout << "直接处理日志 #" << index << " 成功" << std::endl;
            } else {
                std::cerr << "直接处理日志 #" << index << " 失败" << std::endl;
            }
        }
        
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
    
    // 注册信号处理
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // 设置存储
    auto storage = setupStorage();
    
    // 临时测试MySQL连接
    try {
        auto mysqlStorage = storage->GetStorage<xumj::storage::MySQLStorage>("mysql");
        if (mysqlStorage) {
            std::cout << "测试MySQL存储..." << std::endl;
            xumj::storage::MySQLStorage::LogEntry testEntry;
            testEntry.id = "test-log-" + std::to_string(std::time(nullptr));
            testEntry.timestamp = getCurrentTimeStr();
            testEntry.level = "INFO";
            testEntry.source = "complete_system_example";
            testEntry.message = "这是一条测试日志消息";
            
            bool success = mysqlStorage->SaveLogEntry(testEntry);
            if (success) {
                std::cout << "MySQL测试日志写入成功: ID=" << testEntry.id << std::endl;
            } else {
                std::cerr << "MySQL测试日志写入失败" << std::endl;
            }
        } else {
            std::cerr << "无法获取MySQL存储实例" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "MySQL测试异常: " << e.what() << std::endl;
    }
    
    // 设置告警
    auto alertManager = setupAlertManager();
    
    // 设置日志分析
    auto analyzer = setupLogAnalyzer();
    
    // 设置日志处理
    auto processor = setupLogProcessor();
    if (!processor) {
        std::cerr << "日志处理器初始化失败，系统退出" << std::endl;
        return 1;
    }
    
    // 设置TCP服务器
    setupServer(processor, analyzer, alertManager);
    
    // 设置日志收集
    auto collector = setupLogCollector();
    
    // 输出已启动的端口信息
    std::cout << "TCP服务器已启动，监听端口: " << processor->GetConfig().tcpPort << std::endl;
    
    // 启动客户端线程
    std::thread clientThread(runClient, processor);
    
    std::cout << "系统已启动，按Ctrl+C停止..." << std::endl;
    
    // 等待退出信号
    int runTime = 0;
    while (running && runTime < 30) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        runTime++;
        
        // 每5秒输出一个点，表示系统仍在运行
        if (runTime % 5 == 0) {
            std::cout << "." << std::flush;
        }
    }
    
    if (runTime >= 30) {
        std::cout << "\n运行时间达到30秒，系统将自动退出..." << std::endl;
        running = false;
    }
    
    // 等待客户端线程结束
    if (clientThread.joinable()) {
        clientThread.join();
    }
    
    // 停止处理器
    processor->Stop();
    
    return 0;
} 