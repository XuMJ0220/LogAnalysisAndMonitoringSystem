#ifndef XUMJ_LOG_GENERATOR_H
#define XUMJ_LOG_GENERATOR_H

#include <string>
#include <vector>
#include <unordered_map>
#include <random>
#include <chrono>
#include <memory>
#include <functional>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <mutex>
#include <fstream>

namespace xumj {
namespace tools {

/**
 * @brief 日志级别枚举
 */
enum class LogLevel {
    DEBUG,
    INFO,
    WARNING,
    ERROR,
    CRITICAL
};

/**
 * @brief 日志类型枚举
 */
enum class LogType {
    SYSTEM,         // 系统日志
    APPLICATION,    // 应用程序日志
    PERFORMANCE,    // 性能日志
    SECURITY,       // 安全日志
    USER_ACTIVITY,  // 用户活动日志
    DATABASE,       // 数据库日志
    NETWORK,        // 网络日志
    CUSTOM          // 自定义日志
};

// 辅助函数：将LogLevel转换为字符串
std::string LogLevelToString(LogLevel level);

// 辅助函数：将LogType转换为字符串
std::string LogTypeToString(LogType type);

/**
 * @brief 日志生成器配置
 */
struct LogGeneratorConfig {
    // 基本配置
    int logsPerSecond = 1000;                // 每秒生成的日志数量
    int batchSize = 100;                     // 每批次生成的日志数量
    std::chrono::seconds duration{60};       // 生成日志的持续时间
    
    // 日志级别分布 (百分比)
    std::unordered_map<LogLevel, int> levelDistribution = {
        {LogLevel::DEBUG, 20},
        {LogLevel::INFO, 40},
        {LogLevel::WARNING, 25},
        {LogLevel::ERROR, 10},
        {LogLevel::CRITICAL, 5}
    };
    
    // 日志类型分布 (百分比)
    std::unordered_map<LogType, int> typeDistribution = {
        {LogType::SYSTEM, 15},
        {LogType::APPLICATION, 25},
        {LogType::PERFORMANCE, 15},
        {LogType::SECURITY, 10},
        {LogType::USER_ACTIVITY, 20},
        {LogType::DATABASE, 10},
        {LogType::NETWORK, 5}
    };
    
    // 内容复杂度 (1-10, 1最简单，10最复杂)
    int contentComplexity = 5;
    
    // 是否包含结构化数据
    bool includeStructuredData = true;
    
    // 日志发送目标
    std::string targetHost = "127.0.0.1";
    int targetPort = 8000;
    
    // 输出模式
    bool outputToConsole = true;     // 输出到控制台
    bool outputToFile = false;       // 输出到文件
    bool outputToNetwork = false;    // 输出到网络
    std::string outputFilePath = "logs/generated_logs.json";
};

/**
 * @brief 日志生成器接口
 */
class LogGenerator {
public:
    /**
     * @brief 构造函数
     * @param config 日志生成器配置
     */
    explicit LogGenerator(const LogGeneratorConfig& config);
    
    /**
     * @brief 析构函数
     */
    virtual ~LogGenerator();
    
    /**
     * @brief 开始生成日志
     * @return 是否成功启动
     */
    bool Start();
    
    /**
     * @brief 停止生成日志
     */
    void Stop();
    
    /**
     * @brief 等待生成完成
     * @param timeout 超时时间 (毫秒)，0表示无限等待
     * @return 是否在超时前完成
     */
    bool Wait(int timeout = 0);
    
    /**
     * @brief 重置生成器状态
     */
    void Reset();
    
    /**
     * @brief 设置生成日志后的回调函数
     * @param callback 回调函数，接收生成的日志内容
     */
    void SetLogCallback(std::function<void(const std::string&)> callback);
    
    /**
     * @brief 更新配置
     * @param config 新的配置
     */
    void UpdateConfig(const LogGeneratorConfig& config);
    
    /**
     * @brief 获取当前配置
     * @return 当前配置
     */
    LogGeneratorConfig GetConfig() const;
    
    /**
     * @brief 获取已生成的日志数量
     * @return 已生成的日志数量
     */
    uint64_t GetGeneratedLogCount() const;
    
private:
    // 初始化资源
    void InitializeResources();
    
    // 初始化网络连接
    void InitializeNetworkConnection();
    
    // 关闭网络连接
    void CloseNetworkConnection();
    
    // 生成单条日志
    std::string GenerateLog();
    
    // 生成系统日志
    std::string GenerateSystemLog();
    
    // 生成应用日志
    std::string GenerateApplicationLog();
    
    // 生成性能日志
    std::string GeneratePerformanceLog();
    
    // 生成安全日志
    std::string GenerateSecurityLog();
    
    // 生成用户活动日志
    std::string GenerateUserActivityLog();
    
    // 生成数据库日志
    std::string GenerateDatabaseLog();
    
    // 生成网络日志
    std::string GenerateNetworkLog();
    
    // 生成自定义日志
    std::string GenerateCustomLog();
    
    // 根据分布随机选择日志级别
    LogLevel RandomLogLevel();
    
    // 根据分布随机选择日志类型
    LogType RandomLogType();
    
    // 获取日志级别的字符串表示
    std::string LogLevelToString(LogLevel level);
    
    // 获取当前时间的ISO 8601格式字符串
    std::string GetCurrentTimeISO8601();
    
    // 生成随机IP地址
    std::string GenerateRandomIP();
    
    // 生成随机用户ID
    std::string GenerateRandomUserId();
    
    // 生成随机会话ID
    std::string GenerateRandomSessionId();
    
    // 生成随机操作ID
    std::string GenerateRandomOperationId();
    
    // 生成随机的消息内容
    std::string GenerateRandomMessage(LogType type, int complexity);
    
    // 工作线程函数
    void WorkerThread();
    
    // 计时器线程函数
    void TimerThread();
    
    // 写入日志到文件
    void WriteToFile(const std::string& logContent);
    
    // 发送日志到网络
    void SendToNetwork(const std::string& logContent);
    
private:
    LogGeneratorConfig config_;                      // 配置
    std::atomic<bool> running_{false};              // 运行标志
    std::atomic<uint64_t> generatedLogCount_{0};    // 已生成的日志数量
    std::mt19937 rng_;                              // 随机数生成器
    
    std::thread workerThread_;                       // 工作线程
    std::thread timerThread_;                        // 计时器线程
    mutable std::mutex mutex_;                       // 互斥锁
    std::condition_variable cv_;                     // 条件变量
    
    std::function<void(const std::string&)> logCallback_; // 日志回调函数
    
    // 常量和资源
    std::vector<std::string> systemComponents_;      // 系统组件名称
    std::vector<std::string> applicationModules_;    // 应用模块名称
    std::vector<std::string> userActions_;           // 用户操作
    std::vector<std::string> errorMessages_;         // 错误消息
    std::vector<std::string> securityEvents_;        // 安全事件
    
    // 网络连接
    #ifdef _WIN32
    SOCKET networkSocket_;
    #else
    int networkSocket_;
    #endif
};

} // namespace tools
} // namespace xumj

#endif // XUMJ_LOG_GENERATOR_H 