#ifndef XUMJ_PROCESSOR_LOG_PROCESSOR_H
#define XUMJ_PROCESSOR_LOG_PROCESSOR_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>
#include <condition_variable>
#include <iomanip>
#include <unordered_map>
#include <sstream>
#include <iostream>

#include "xumj/analyzer/log_analyzer.h"
#include "xumj/storage/redis_storage.h"
#include "xumj/storage/mysql_storage.h"
#include "xumj/network/tcp_server.h"
#include "xumj/common/non_copyable.h"

// 引入Muduo TCP连接相关类型
#include <muduo/net/TcpConnection.h>

namespace xumj {
namespace processor {

// 使用Muduo网络库的TCP连接类型
using TcpConnectionPtr = muduo::net::TcpConnectionPtr;

/**
 * @struct LogData
 * @brief 日志数据结构，包含原始日志信息和元数据
 */
struct LogData {
    std::string id;                                     ///< 日志ID
    std::string message;                                ///< 日志消息内容
    std::string source;                                 ///< 日志来源
    std::chrono::system_clock::time_point timestamp;    ///< 时间戳
    std::unordered_map<std::string, std::string> metadata;  ///< 元数据
};

/**
 * @class LogParser
 * @brief 日志解析器接口，用于将日志数据解析为标准格式
 */
class LogParser {
public:
    virtual ~LogParser() = default;
    
    /**
     * @brief 获取解析器类型
     * @return 解析器类型
     */
    virtual std::string GetType() const { return "BaseParser"; }
    
    /**
     * @brief 解析日志数据
     * @param logData 日志数据
     * @param record 解析结果
     * @return 是否成功解析
     */
    virtual bool Parse(const LogData& logData, analyzer::LogRecord& record) = 0;
};

/**
 * @class JsonLogParser
 * @brief JSON日志解析器，解析JSON格式的日志
 */
class JsonLogParser : public LogParser {
public:
    JsonLogParser() = default;
    
    /**
     * @brief 获取解析器类型
     * @return 解析器类型
     */
    virtual std::string GetType() const override { return "JsonParser"; }
    
    /**
     * @brief 解析JSON格式的日志数据
     * @param logData 日志数据
     * @param record 解析结果
     * @return 是否成功解析
     */
    virtual bool Parse(const LogData& logData, analyzer::LogRecord& record) override;
};

/**
 * @struct LogProcessorConfig
 * @brief 日志处理器配置
 */
struct LogProcessorConfig {
    bool debug{false};                  ///< 是否启用调试模式
    int workerThreads{4};               ///< 工作线程数
    int queueSize{10000};               ///< 队列大小
    int tcpPort{8001};                  ///< TCP服务器端口
    bool enableRedisStorage{true};      ///< 是否启用Redis存储
    bool enableMySQLStorage{true};      ///< 是否启用MySQL存储
};

/**
 * @class LogProcessor
 * @brief 日志处理器，负责接收、解析、处理和存储日志数据
 */
class LogProcessor : public common::NonCopyable {
public:
    /**
     * @brief 构造函数
     * @param config 日志处理器配置
     */
    explicit LogProcessor(const LogProcessorConfig& config = LogProcessorConfig());
    
    /**
     * @brief 析构函数
     */
    ~LogProcessor();
    
    /**
     * @brief 启动日志处理器
     * @return 是否成功启动
     */
    bool Start();
    
    /**
     * @brief 停止日志处理器
     */
    void Stop();
    
    /**
     * @brief 添加日志解析器
     * @param parser 日志解析器
     */
    void AddLogParser(std::shared_ptr<LogParser> parser);
    
    /**
     * @brief 提交日志数据
     * @param data 日志数据
     * @return 是否成功提交
     */
    bool SubmitLogData(const LogData& data);
    
    /**
     * @brief 获取待处理数据数量
     * @return 待处理数据数量
     */
    size_t GetPendingCount() const;
    
    /**
     * @brief 设置日志分析器
     * @param analyzer 日志分析器
     */
    void SetAnalyzer(std::shared_ptr<analyzer::LogAnalyzer> analyzer) {
        analyzer_ = analyzer;
    }
    
    /**
     * @brief 获取日志分析器
     * @return 日志分析器
     */
    std::shared_ptr<analyzer::LogAnalyzer> GetAnalyzer() const {
        return analyzer_;
    }
    
    /**
     * @brief 获取处理器配置
     * @return 处理器配置
     */
    const LogProcessorConfig& GetConfig() const {
        return config_;
    }
    
    /**
     * @brief 直接处理JSON字符串
     * @param jsonStr JSON字符串
     * @return 是否成功处理
     */
    bool ProcessJsonString(const std::string& jsonStr);
    
private:
    LogProcessorConfig config_;                         // 配置
    std::atomic<bool> running_{false};                  // 运行状态
    
    // 日志解析器
    std::vector<std::shared_ptr<LogParser>> parsers_;   // 日志解析器列表
    mutable std::mutex parsersMutex_;                   // 解析器互斥锁
    
    // 数据队列
    std::queue<LogData> logQueue_;                      // 日志数据队列
    mutable std::mutex queueMutex_;                     // 队列互斥锁
    std::condition_variable queueCondVar_;              // 队列条件变量
    std::atomic<size_t> dataCount_{0};                  // 数据计数器
    
    // 网络
    std::unique_ptr<network::TcpServer> tcpServer_;     // TCP服务器
    std::unordered_map<uint64_t, std::string> connections_;  // 连接列表
    mutable std::mutex connectionsMutex_;               // 连接互斥锁
    
    // 存储
    std::shared_ptr<storage::RedisStorage> redisStorage_;  // Redis存储
    std::shared_ptr<storage::MySQLStorage> mysqlStorage_;  // MySQL存储
    
    // 分析
    std::shared_ptr<analyzer::LogAnalyzer> analyzer_;   // 日志分析器
    
    // 工作线程
    std::vector<std::thread> workers_;                  // 工作线程列表
    
    /**
     * @brief 工作线程函数
     */
    void WorkerThread();
    
    /**
     * @brief 初始化TCP服务器
     * @return 是否成功初始化
     */
    bool InitializeTcpServer();
    
    /**
     * @brief 处理TCP连接
     * @param connectionId 连接ID
     * @param clientAddr 客户端地址
     * @param connected 是否已连接
     */
    void HandleTcpConnection(uint64_t connectionId, const std::string& clientAddr, bool connected);
    
    /**
     * @brief 处理TCP消息
     * @param conn TCP连接
     * @param message 消息内容
     */
    void HandleTcpMessage(const TcpConnectionPtr& conn, const std::string& message);
    
    /**
     * @brief 处理日志数据
     * @param logData 日志数据
     */
    void ProcessLogData(LogData logData);
    
    /**
     * @brief 存储日志记录到Redis
     * @param record 日志记录
     */
    void StoreRedisLog(const analyzer::LogRecord& record);
    
    /**
     * @brief 存储日志记录到MySQL
     * @param record 日志记录
     */
    void StoreMySQLLog(const analyzer::LogRecord& record);
};

/**
 * @brief 生成一个UUID
 * @return UUID字符串
 */
std::string GenerateUUID();

/**
 * @brief 将时间点转换为字符串
 * @param tp 时间点
 * @return 格式化的时间字符串
 */
std::string TimestampToString(const std::chrono::system_clock::time_point& tp);

} // namespace processor
} // namespace xumj

#endif // XUMJ_PROCESSOR_LOG_PROCESSOR_H 