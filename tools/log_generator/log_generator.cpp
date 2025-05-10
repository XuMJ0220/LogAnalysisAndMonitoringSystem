#include "log_generator.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <algorithm>
#include <random>
#include <chrono>
#include <thread>
#include <filesystem>
#include <cstring>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#endif

namespace xumj {
namespace tools {

// 将LogLevel转换为字符串
std::string LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// 将LogType转换为字符串
std::string LogTypeToString(LogType type) {
    switch (type) {
        case LogType::SYSTEM: return "SYSTEM";
        case LogType::APPLICATION: return "APPLICATION";
        case LogType::PERFORMANCE: return "PERFORMANCE";
        case LogType::SECURITY: return "SECURITY";
        case LogType::USER_ACTIVITY: return "USER_ACTIVITY";
        case LogType::DATABASE: return "DATABASE";
        case LogType::NETWORK: return "NETWORK";
        case LogType::CUSTOM: return "CUSTOM";
        default: return "UNKNOWN";
    }
}

// 构造函数
LogGenerator::LogGenerator(const LogGeneratorConfig& config)
    : config_(config), networkSocket_(-1) {
    
    // 初始化随机数生成器
    std::random_device rd;
    rng_ = std::mt19937(rd());
    
    // 初始化资源
    InitializeResources();
    
    // 如果需要网络输出，初始化网络连接
    if (config_.outputToNetwork) {
        InitializeNetworkConnection();
    }
}

// 析构函数
LogGenerator::~LogGenerator() {
    Stop();
    Wait();
    
    // 关闭网络连接
    CloseNetworkConnection();
}

// 初始化网络连接
void LogGenerator::InitializeNetworkConnection() {
#ifdef _WIN32
    // Windows 初始化
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup失败" << std::endl;
        return;
    }
    
    networkSocket_ = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (networkSocket_ == INVALID_SOCKET) {
        std::cerr << "创建socket失败" << std::endl;
        WSACleanup();
        return;
    }
    
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(config_.targetPort);
    serverAddr.sin_addr.s_addr = inet_addr(config_.targetHost.c_str());
    
    if (connect(networkSocket_, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "连接到服务器失败: " << config_.targetHost << ":" << config_.targetPort << std::endl;
        closesocket(networkSocket_);
        networkSocket_ = INVALID_SOCKET;
        WSACleanup();
    }
#else
    // Linux/Unix 初始化
    networkSocket_ = socket(AF_INET, SOCK_STREAM, 0);
    if (networkSocket_ < 0) {
        std::cerr << "创建socket失败" << std::endl;
        return;
    }
    
    struct sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(config_.targetPort);
    serverAddr.sin_addr.s_addr = inet_addr(config_.targetHost.c_str());
    
    if (connect(networkSocket_, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0) {
        std::cerr << "连接到服务器失败: " << config_.targetHost << ":" << config_.targetPort << std::endl;
        close(networkSocket_);
        networkSocket_ = -1;
    } else {
        std::cout << "已连接到服务器: " << config_.targetHost << ":" << config_.targetPort << std::endl;
    }
#endif
}

// 关闭网络连接
void LogGenerator::CloseNetworkConnection() {
#ifdef _WIN32
    if (networkSocket_ != INVALID_SOCKET) {
        closesocket(networkSocket_);
        networkSocket_ = INVALID_SOCKET;
        WSACleanup();
    }
#else
    if (networkSocket_ >= 0) {
        close(networkSocket_);
        networkSocket_ = -1;
    }
#endif
}

// 初始化资源
void LogGenerator::InitializeResources() {
    // 系统组件名称
    systemComponents_ = {
        "内核", "文件系统", "网络堆栈", "内存管理", "进程调度器", 
        "设备驱动", "I/O子系统", "安全模块", "电源管理", "系统服务"
    };
    
    // 应用模块名称
    applicationModules_ = {
        "用户界面", "数据处理", "报表生成", "认证服务", "配置管理",
        "缓存服务", "通知系统", "任务调度", "资源监控", "API网关",
        "数据验证", "业务逻辑", "工作流引擎", "支付处理", "订单管理"
    };
    
    // 用户操作
    userActions_ = {
        "登录", "登出", "创建记录", "删除记录", "更新配置",
        "上传文件", "下载报表", "发送消息", "更改密码", "浏览产品",
        "添加购物车", "完成订单", "发表评论", "分享内容", "关注用户"
    };
    
    // 错误消息
    errorMessages_ = {
        "连接超时", "认证失败", "权限不足", "资源不存在", "服务不可用",
        "数据格式错误", "资源已锁定", "操作被拒绝", "并发冲突", "系统过载",
        "内存不足", "磁盘空间不足", "网络中断", "数据库错误", "依赖服务失败"
    };
    
    // 安全事件
    securityEvents_ = {
        "失败的登录尝试", "权限提升", "配置修改", "敏感数据访问", "用户账户变更",
        "异常登录位置", "恶意软件检测", "DDoS攻击尝试", "扫描行为", "数据泄露尝试",
        "防火墙规则变更", "证书异常", "未授权API访问", "会话劫持尝试", "SQL注入尝试"
    };
}

// 开始生成日志
bool LogGenerator::Start() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    if (running_) {
        return false;  // 已经在运行
    }
    
    running_ = true;
    generatedLogCount_ = 0;
    
    // 确保输出目录存在（如果需要输出到文件）
    if (config_.outputToFile) {
        std::filesystem::path filePath(config_.outputFilePath);
        std::filesystem::create_directories(filePath.parent_path());
    }
    
    // 启动工作线程
    workerThread_ = std::thread(&LogGenerator::WorkerThread, this);
    
    // 如果有持续时间限制，启动计时器线程
    if (config_.duration.count() > 0) {
        timerThread_ = std::thread(&LogGenerator::TimerThread, this);
    }
    
    return true;
}

// 停止生成日志
void LogGenerator::Stop() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!running_) {
            return;  // 已经停止
        }
        running_ = false;
    }
    
    cv_.notify_all();  // 唤醒所有等待的线程
}

// 等待生成完成
bool LogGenerator::Wait(int timeout) {
    if (workerThread_.joinable()) {
        if (timeout > 0) {
            auto endTime = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeout);
            std::unique_lock<std::mutex> lock(mutex_);
            return cv_.wait_until(lock, endTime, [this] { return !running_; });
        } else {
            workerThread_.join();
        }
    }
    
    if (timerThread_.joinable()) {
        timerThread_.join();
    }
    
    return !running_;
}

// 重置生成器状态
void LogGenerator::Reset() {
    Stop();
    Wait();
    
    std::lock_guard<std::mutex> lock(mutex_);
    generatedLogCount_ = 0;
}

// 设置日志回调函数
void LogGenerator::SetLogCallback(std::function<void(const std::string&)> callback) {
    std::lock_guard<std::mutex> lock(mutex_);
    logCallback_ = callback;
}

// 更新配置
void LogGenerator::UpdateConfig(const LogGeneratorConfig& config) {
    Stop();
    Wait();
    
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

// 获取当前配置
LogGeneratorConfig LogGenerator::GetConfig() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

// 获取已生成的日志数量
uint64_t LogGenerator::GetGeneratedLogCount() const {
    return generatedLogCount_;
}

// 获取当前时间的ISO 8601格式字符串
std::string LogGenerator::GetCurrentTimeISO8601() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = *std::localtime(&time_t_now);
    
    char buffer[30];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_now);
    
    return std::string(buffer);
}

// 生成随机IP地址
std::string LogGenerator::GenerateRandomIP() {
    std::uniform_int_distribution<int> dist(1, 254);
    
    return std::to_string(dist(rng_)) + "." + 
           std::to_string(dist(rng_)) + "." + 
           std::to_string(dist(rng_)) + "." + 
           std::to_string(dist(rng_));
}

// 生成随机用户ID
std::string LogGenerator::GenerateRandomUserId() {
    std::uniform_int_distribution<int> dist(1000, 9999);
    return "user_" + std::to_string(dist(rng_));
}

// 生成随机会话ID
std::string LogGenerator::GenerateRandomSessionId() {
    std::uniform_int_distribution<int> dist(10000, 99999);
    return "session_" + std::to_string(dist(rng_));
}

// 生成随机操作ID
std::string LogGenerator::GenerateRandomOperationId() {
    std::uniform_int_distribution<int> dist(100000, 999999);
    return "op_" + std::to_string(dist(rng_));
}

// 根据分布随机选择日志级别
LogLevel LogGenerator::RandomLogLevel() {
    std::vector<std::pair<LogLevel, int>> distribution;
    int total = 0;
    
    for (const auto& pair : config_.levelDistribution) {
        distribution.push_back(pair);
        total += pair.second;
    }
    
    std::uniform_int_distribution<int> dist(1, total);
    int rand = dist(rng_);
    
    int accumulator = 0;
    for (const auto& pair : distribution) {
        accumulator += pair.second;
        if (rand <= accumulator) {
            return pair.first;
        }
    }
    
    return LogLevel::INFO;  // 默认
}

// 根据分布随机选择日志类型
LogType LogGenerator::RandomLogType() {
    std::vector<std::pair<LogType, int>> distribution;
    int total = 0;
    
    for (const auto& pair : config_.typeDistribution) {
        distribution.push_back(pair);
        total += pair.second;
    }
    
    std::uniform_int_distribution<int> dist(1, total);
    int rand = dist(rng_);
    
    int accumulator = 0;
    for (const auto& pair : distribution) {
        accumulator += pair.second;
        if (rand <= accumulator) {
            return pair.first;
        }
    }
    
    return LogType::APPLICATION;  // 默认
}

// 工作线程函数
void LogGenerator::WorkerThread() {
    // 添加安全检查，避免除以零错误
    if (config_.logsPerSecond <= 0) {
        std::cerr << "错误: 日志生成速率必须大于0" << std::endl;
        running_ = false;
        return;
    }
    
    if (config_.batchSize <= 0) {
        std::cerr << "错误: 批处理大小必须大于0" << std::endl;
        running_ = false;
        return;
    }
    
    // 如果需要网络输出但连接失败，停止生成
    if (config_.outputToNetwork && networkSocket_ < 0) {
        std::cerr << "错误: 无法连接到服务器，日志生成停止" << std::endl;
        running_ = false;
        return;
    }
    
    const auto batchIntervalNs = std::chrono::nanoseconds(1'000'000'000) / 
                                (config_.logsPerSecond / config_.batchSize);
    
    auto nextBatchTime = std::chrono::steady_clock::now();
    
    while (running_) {
        // 生成一批日志
        for (int i = 0; i < config_.batchSize && running_; ++i) {
            std::string logContent = GenerateLog();
            
            // 输出到控制台
            if (config_.outputToConsole) {
                std::cout << logContent << std::endl;
            }
            
            // 输出到文件
            if (config_.outputToFile) {
                WriteToFile(logContent);
            }
            
            // 输出到网络
            if (config_.outputToNetwork) {
                SendToNetwork(logContent);
            }
            
            // 调用回调函数
            if (logCallback_) {
                logCallback_(logContent);
            }
            
            generatedLogCount_++;
        }
        
        // 计算下一批次的时间
        nextBatchTime += batchIntervalNs;
        
        // 睡眠到下一批次
        std::this_thread::sleep_until(nextBatchTime);
    }
}

// 计时器线程函数
void LogGenerator::TimerThread() {
    std::this_thread::sleep_for(config_.duration);
    Stop();
}

// 生成单条日志
std::string LogGenerator::GenerateLog() {
    LogType type = RandomLogType();
    LogLevel level = RandomLogLevel();
    
    std::string logContent;
    
    switch (type) {
        case LogType::SYSTEM:
            logContent = GenerateSystemLog();
            break;
        case LogType::APPLICATION:
            logContent = GenerateApplicationLog();
            break;
        case LogType::PERFORMANCE:
            logContent = GeneratePerformanceLog();
            break;
        case LogType::SECURITY:
            logContent = GenerateSecurityLog();
            break;
        case LogType::USER_ACTIVITY:
            logContent = GenerateUserActivityLog();
            break;
        case LogType::DATABASE:
            logContent = GenerateDatabaseLog();
            break;
        case LogType::NETWORK:
            logContent = GenerateNetworkLog();
            break;
        case LogType::CUSTOM:
            logContent = GenerateCustomLog();
            break;
        default:
            logContent = GenerateApplicationLog();
            break;
    }
    
    return logContent;
}

// 生成系统日志
std::string LogGenerator::GenerateSystemLog() {
    std::uniform_int_distribution<int> dist(0, systemComponents_.size() - 1);
    std::uniform_int_distribution<int> cpu_dist(0, 100);
    std::uniform_int_distribution<int> mem_dist(0, 100);
    std::uniform_int_distribution<int> disk_dist(0, 100);
    
    LogLevel level = RandomLogLevel();
    std::string component = systemComponents_[dist(rng_)];
    int cpuUsage = cpu_dist(rng_);
    int memoryUsage = mem_dist(rng_);
    int diskUsage = disk_dist(rng_);
    
    std::ostringstream json;
    json << "{"
         << "\"timestamp\":\"" << GetCurrentTimeISO8601() << "\","
         << "\"type\":\"SYSTEM\","
         << "\"level\":\"" << LogLevelToString(level) << "\","
         << "\"component\":\"" << component << "\","
         << "\"message\":\"系统资源使用监控\","
         << "\"cpu_usage\":" << cpuUsage << ","
         << "\"memory_usage\":" << memoryUsage << ","
         << "\"disk_usage\":" << diskUsage << ","
         << "\"hostname\":\"server-" << (dist(rng_) % 10) << "\","
         << "\"pid\":" << (10000 + dist(rng_) % 10000)
         << "}";
    
    return json.str();
}

// 生成应用日志
std::string LogGenerator::GenerateApplicationLog() {
    std::uniform_int_distribution<int> dist(0, applicationModules_.size() - 1);
    std::uniform_int_distribution<int> session_dist(1000, 9999);
    
    LogLevel level = RandomLogLevel();
    std::string module = applicationModules_[dist(rng_)];
    std::string sessionId = "session-" + std::to_string(session_dist(rng_));
    std::string userId = GenerateRandomUserId();
    
    std::ostringstream json;
    json << "{"
         << "\"timestamp\":\"" << GetCurrentTimeISO8601() << "\","
         << "\"type\":\"APPLICATION\","
         << "\"level\":\"" << LogLevelToString(level) << "\","
         << "\"module\":\"" << module << "\","
         << "\"message\":\"应用程序运行状态\","
         << "\"session_id\":\"" << sessionId << "\","
         << "\"user_id\":\"" << userId << "\","
         << "\"client_ip\":\"" << GenerateRandomIP() << "\","
         << "\"request_id\":\"req-" << (dist(rng_) % 1000) << "\""
         << "}";
    
    return json.str();
}

// 生成性能日志
std::string LogGenerator::GeneratePerformanceLog() {
    std::uniform_int_distribution<int> time_dist(5, 5000);
    std::uniform_int_distribution<int> rows_dist(0, 10000);
    std::uniform_int_distribution<int> dist(0, 9);
    
    LogLevel level = RandomLogLevel();
    int queryTime = time_dist(rng_);
    int rowsExamined = rows_dist(rng_);
    std::string queryId = "Q" + std::to_string(dist(rng_)) + std::to_string(dist(rng_)) + std::to_string(dist(rng_));
    
    std::ostringstream json;
    json << "{"
         << "\"timestamp\":\"" << GetCurrentTimeISO8601() << "\","
         << "\"type\":\"PERFORMANCE\","
         << "\"level\":\"" << LogLevelToString(level) << "\","
         << "\"message\":\"性能指标监控\","
         << "\"query_time\":" << queryTime << ","
         << "\"query_id\":\"" << queryId << "\","
         << "\"rows_examined\":" << rowsExamined << ","
         << "\"database\":\"db" << (dist(rng_) % 5) << "\","
         << "\"operation\":\"" << (dist(rng_) % 2 == 0 ? "read" : "write") << "\""
         << "}";
    
    return json.str();
}

// 生成安全日志
std::string LogGenerator::GenerateSecurityLog() {
    std::uniform_int_distribution<int> dist(0, securityEvents_.size() - 1);
    std::uniform_int_distribution<int> success_dist(0, 9);
    
    LogLevel level = RandomLogLevel();
    std::string event = securityEvents_[dist(rng_)];
    bool success = success_dist(rng_) < 7;  // 70% 成功率
    
    std::ostringstream json;
    json << "{"
         << "\"timestamp\":\"" << GetCurrentTimeISO8601() << "\","
         << "\"type\":\"SECURITY\","
         << "\"level\":\"" << LogLevelToString(level) << "\","
         << "\"message\":\"" << event << "\","
         << "\"success\":" << (success ? "true" : "false") << ","
         << "\"user_id\":\"" << GenerateRandomUserId() << "\","
         << "\"ip_address\":\"" << GenerateRandomIP() << "\","
         << "\"location\":\"" << (dist(rng_) % 5 == 0 ? "异常位置" : "正常位置") << "\","
         << "\"action_id\":\"" << GenerateRandomOperationId() << "\""
         << "}";
    
    return json.str();
}

// 生成用户活动日志
std::string LogGenerator::GenerateUserActivityLog() {
    std::uniform_int_distribution<int> dist(0, userActions_.size() - 1);
    std::uniform_int_distribution<int> duration_dist(10, 5000);
    
    LogLevel level = RandomLogLevel();
    std::string action = userActions_[dist(rng_)];
    int duration = duration_dist(rng_);
    
    std::ostringstream json;
    json << "{"
         << "\"timestamp\":\"" << GetCurrentTimeISO8601() << "\","
         << "\"type\":\"USER_ACTIVITY\","
         << "\"level\":\"" << LogLevelToString(level) << "\","
         << "\"message\":\"用户活动记录\","
         << "\"user_id\":\"" << GenerateRandomUserId() << "\","
         << "\"action\":\"" << action << "\","
         << "\"duration\":" << duration << ","
         << "\"session_id\":\"" << GenerateRandomSessionId() << "\","
         << "\"ip_address\":\"" << GenerateRandomIP() << "\","
         << "\"user_agent\":\"Mozilla/5.0 (" << (dist(rng_) % 2 == 0 ? "Windows" : "Linux") << ")\""
         << "}";
    
    return json.str();
}

// 生成数据库日志
std::string LogGenerator::GenerateDatabaseLog() {
    std::uniform_int_distribution<int> dist(0, 9);
    std::uniform_int_distribution<int> rows_dist(0, 1000);
    
    LogLevel level = RandomLogLevel();
    std::string operation = dist(rng_) % 4 == 0 ? "SELECT" : (dist(rng_) % 3 == 0 ? "INSERT" : (dist(rng_) % 2 == 0 ? "UPDATE" : "DELETE"));
    int rowsAffected = rows_dist(rng_);
    
    std::ostringstream json;
    json << "{"
         << "\"timestamp\":\"" << GetCurrentTimeISO8601() << "\","
         << "\"type\":\"DATABASE\","
         << "\"level\":\"" << LogLevelToString(level) << "\","
         << "\"message\":\"数据库操作记录\","
         << "\"operation\":\"" << operation << "\","
         << "\"table\":\"table" << (dist(rng_) % 10) << "\","
         << "\"rows_affected\":" << rowsAffected << ","
         << "\"database\":\"db" << (dist(rng_) % 5) << "\","
         << "\"query_id\":\"" << GenerateRandomOperationId() << "\","
         << "\"user_id\":\"" << GenerateRandomUserId() << "\""
         << "}";
    
    return json.str();
}

// 生成网络日志
std::string LogGenerator::GenerateNetworkLog() {
    std::uniform_int_distribution<int> dist(0, 9);
    std::uniform_int_distribution<int> bytes_dist(100, 10000000);
    std::uniform_int_distribution<int> status_dist(200, 599);
    
    LogLevel level = RandomLogLevel();
    int statusCode = status_dist(rng_);
    if (statusCode >= 200 && statusCode < 300) statusCode = 200 + (statusCode % 6);
    if (statusCode >= 300 && statusCode < 400) statusCode = 300 + (statusCode % 8);
    if (statusCode >= 400 && statusCode < 500) statusCode = 400 + (statusCode % 10);
    if (statusCode >= 500) statusCode = 500 + (statusCode % 6);
    
    std::string method = dist(rng_) % 5 == 0 ? "GET" : (dist(rng_) % 4 == 0 ? "POST" : (dist(rng_) % 3 == 0 ? "PUT" : (dist(rng_) % 2 == 0 ? "DELETE" : "PATCH")));
    int bytesTransferred = bytes_dist(rng_);
    
    std::ostringstream json;
    json << "{"
         << "\"timestamp\":\"" << GetCurrentTimeISO8601() << "\","
         << "\"type\":\"NETWORK\","
         << "\"level\":\"" << LogLevelToString(level) << "\","
         << "\"message\":\"网络请求记录\","
         << "\"method\":\"" << method << "\","
         << "\"path\":\"/api/resource" << (dist(rng_) % 10) << "/action" << (dist(rng_) % 5) << "\","
         << "\"status_code\":" << statusCode << ","
         << "\"bytes\":" << bytesTransferred << ","
         << "\"client_ip\":\"" << GenerateRandomIP() << "\","
         << "\"response_time\":" << (dist(rng_) % 1000) << ","
         << "\"user_agent\":\"Mozilla/5.0\""
         << "}";
    
    return json.str();
}

// 生成自定义日志
std::string LogGenerator::GenerateCustomLog() {
    std::uniform_int_distribution<int> dist(0, 9);
    
    LogLevel level = RandomLogLevel();
    
    std::ostringstream json;
    json << "{"
         << "\"timestamp\":\"" << GetCurrentTimeISO8601() << "\","
         << "\"type\":\"CUSTOM\","
         << "\"level\":\"" << LogLevelToString(level) << "\","
         << "\"message\":\"自定义日志消息 #" << dist(rng_) << "\","
         << "\"custom_field1\":\"值" << dist(rng_) << "\","
         << "\"custom_field2\":" << (dist(rng_) * 100) << ","
         << "\"custom_field3\":\"" << GenerateRandomOperationId() << "\","
         << "\"custom_field4\":" << (dist(rng_) % 2 == 0 ? "true" : "false") << ","
         << "\"custom_timestamp\":\"" << GetCurrentTimeISO8601() << "\""
         << "}";
    
    return json.str();
}

// 写入日志到文件
void LogGenerator::WriteToFile(const std::string& logContent) {
    std::ofstream file(config_.outputFilePath, std::ios::app);
    if (file.is_open()) {
        file << logContent << std::endl;
        file.close();
    }
}

// 发送日志到网络
void LogGenerator::SendToNetwork(const std::string& logContent) {
    // 尝试使用回调函数
    if (logCallback_) {
        std::cout << "通过回调函数发送网络消息，长度: " << logContent.size() 
                  << "，预览: " << (logContent.size() > 30 ? logContent.substr(0, 30) + "..." : logContent)
                  << std::endl;
        logCallback_(logContent);
        return;
    }
    
    // 下面是回退机制，如果没有设置回调函数，则使用原生 socket API
    std::cout << "使用原生Socket API发送网络消息" << std::endl;
    
    // 如果没有有效的网络连接，尝试重新连接
    if (
#ifdef _WIN32
        networkSocket_ == INVALID_SOCKET
#else
        networkSocket_ < 0
#endif
    ) {
        std::cout << "没有有效的网络连接，尝试重新连接..." << std::endl;
        InitializeNetworkConnection();
        // 如果重连失败，直接返回
        if (
#ifdef _WIN32
            networkSocket_ == INVALID_SOCKET
#else
            networkSocket_ < 0
#endif
        ) {
            std::cerr << "重连失败，无法发送日志" << std::endl;
            return;
        }
        std::cout << "重连成功，继续发送日志" << std::endl;
    }

    // 发送数据
    int sendResult;
#ifdef _WIN32
    sendResult = send(networkSocket_, logContent.c_str(), static_cast<int>(logContent.size()), 0);
    if (sendResult == SOCKET_ERROR) {
        std::cerr << "发送失败，错误码: " << WSAGetLastError() << std::endl;
        // 发送失败，关闭连接
        closesocket(networkSocket_);
        networkSocket_ = INVALID_SOCKET;
    } else {
        std::cout << "成功发送 " << sendResult << " 字节" << std::endl;
    }
#else
    sendResult = send(networkSocket_, logContent.c_str(), logContent.size(), 0);
    if (sendResult < 0) {
        std::cerr << "发送失败，错误码: " << errno << ", " << strerror(errno) << std::endl;
        // 发送失败，关闭连接
        close(networkSocket_);
        networkSocket_ = -1;
    } else {
        std::cout << "成功发送 " << sendResult << " 字节" << std::endl;
    }
#endif
}

// 获取日志级别的字符串表示
std::string LogGenerator::LogLevelToString(LogLevel level) {
    return ::xumj::tools::LogLevelToString(level);
}

} // namespace tools
} // namespace xumj 