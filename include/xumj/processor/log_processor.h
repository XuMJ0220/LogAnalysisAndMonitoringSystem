#ifndef XUMJ_PROCESSOR_LOG_PROCESSOR_H
#define XUMJ_PROCESSOR_LOG_PROCESSOR_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>
#include "xumj/analyzer/log_analyzer.h"
#include "xumj/storage/redis_storage.h"
#include "xumj/storage/mysql_storage.h"
#include "xumj/network/tcp_server.h"
#include "xumj/common/thread_pool.h"

namespace xumj {
namespace processor {

/**
 * @struct LogData
 * @brief 日志数据结构，包含原始日志数据和元信息
 */
struct LogData {
    std::string id;                ///< 日志ID
    std::string data;              ///< 原始日志数据
    std::string source;            ///< 日志来源（IP或标识符）
    std::chrono::system_clock::time_point timestamp;  ///< 接收时间戳
    bool compressed{false};        ///< 是否已压缩
    std::unordered_map<std::string, std::string> metadata;  ///< 元数据
};

/**
 * @class LogParser
 * @brief 日志解析器接口
 */
class LogParser {
public:
    virtual ~LogParser() = default;
    
    /**
     * @brief 解析日志数据
     * @param data 原始日志数据
     * @return 解析后的日志记录
     */
    virtual analyzer::LogRecord Parse(const LogData& data) const = 0;
    
    /**
     * @brief 获取解析器名称
     * @return 解析器名称
     */
    virtual std::string GetName() const = 0;
    
    /**
     * @brief 检查是否可以解析指定格式
     * @param data 日志数据
     * @return 是否可以解析
     */
    virtual bool CanParse(const LogData& data) const = 0;
};

/**
 * @class RegexLogParser
 * @brief 基于正则表达式的日志解析器
 */
class RegexLogParser : public LogParser {
public:
    /**
     * @brief 构造函数
     * @param name 解析器名称
     * @param pattern 正则表达式模式
     * @param fieldMapping 匹配组到字段的映射
     */
    RegexLogParser(const std::string& name,
                  const std::string& pattern,
                  const std::unordered_map<int, std::string>& fieldMapping);
    
    /**
     * @brief 解析日志数据
     * @param data 原始日志数据
     * @return 解析后的日志记录
     */
    analyzer::LogRecord Parse(const LogData& data) const override;
    
    /**
     * @brief 获取解析器名称
     * @return 解析器名称
     */
    std::string GetName() const override;
    
    /**
     * @brief 检查是否可以解析指定格式
     * @param data 日志数据
     * @return 是否可以解析
     */
    bool CanParse(const LogData& data) const override;
    
private:
    std::string name_;               ///< 解析器名称
    std::string pattern_;            ///< 正则表达式模式
    std::unordered_map<int, std::string> fieldMapping_;  ///< 匹配组到字段的映射
    // 实际实现中可能需要预编译的正则表达式对象
};

/**
 * @class JsonLogParser
 * @brief 基于JSON格式的日志解析器
 */
class JsonLogParser : public LogParser {
public:
    /**
     * @brief 构造函数
     * @param name 解析器名称
     * @param fieldMapping JSON字段到日志记录字段的映射
     */
    JsonLogParser(const std::string& name,
                 const std::unordered_map<std::string, std::string>& fieldMapping);
    
    /**
     * @brief 解析日志数据
     * @param data 原始日志数据
     * @return 解析后的日志记录
     */
    analyzer::LogRecord Parse(const LogData& data) const override;
    
    /**
     * @brief 获取解析器名称
     * @return 解析器名称
     */
    std::string GetName() const override;
    
    /**
     * @brief 检查是否可以解析指定格式
     * @param data 日志数据
     * @return 是否可以解析
     */
    bool CanParse(const LogData& data) const override;
    
private:
    std::string name_;               ///< 解析器名称
    std::unordered_map<std::string, std::string> fieldMapping_;  ///< JSON字段到日志记录字段的映射
};

/**
 * @struct ProcessorConfig
 * @brief 日志处理器配置
 */
struct ProcessorConfig {
    size_t threadPoolSize{8};                 ///< 处理线程池大小
    std::chrono::seconds processInterval{1};  ///< 处理间隔时间
    size_t batchSize{200};                    ///< 每批处理的日志数量
    size_t maxQueueSize{10000};               ///< 最大队列大小
    bool compressArchive{true};               ///< 是否压缩存档
    std::string redisConfigJson{};            ///< Redis配置JSON
    std::string mysqlConfigJson{};            ///< MySQL配置JSON
    analyzer::AnalyzerConfig analyzerConfig{};  ///< 分析器配置
};

/**
 * @class LogProcessor
 * @brief 日志处理器，负责接收、解析、处理和分发日志数据
 */
class LogProcessor {
public:
    /**
     * @brief 日志处理完成回调函数类型
     * @param logId 日志ID
     * @param success 是否处理成功
     */
    using ProcessCallback = std::function<void(const std::string&, bool)>;
    
    /**
     * @brief 构造函数
     * @param config 处理器配置
     */
    explicit LogProcessor(const ProcessorConfig& config = ProcessorConfig());
    
    /**
     * @brief 析构函数
     */
    ~LogProcessor();
    
    /**
     * @brief 初始化处理器
     * @param config 处理器配置
     * @return 是否成功初始化
     */
    bool Initialize(const ProcessorConfig& config);
    
    /**
     * @brief 添加日志解析器
     * @param parser 解析器
     */
    void AddParser(std::shared_ptr<LogParser> parser);
    
    /**
     * @brief 清除所有解析器
     */
    void ClearParsers();
    
    /**
     * @brief 提交日志数据进行处理
     * @param data 日志数据
     * @return 是否成功提交
     */
    bool SubmitLogData(const LogData& data);
    
    /**
     * @brief 批量提交日志数据进行处理
     * @param dataList 日志数据列表
     * @return 成功提交的数据数量
     */
    size_t SubmitLogDataBatch(const std::vector<LogData>& dataList);
    
    /**
     * @brief 设置处理完成回调函数
     * @param callback 回调函数
     */
    void SetProcessCallback(ProcessCallback callback);
    
    /**
     * @brief 启动处理器
     * @return 是否成功启动
     */
    bool Start();
    
    /**
     * @brief 停止处理器
     */
    void Stop();
    
    /**
     * @brief 获取处理器是否正在运行
     * @return 是否正在运行
     */
    bool IsRunning() const;
    
    /**
     * @brief 获取当前解析器数量
     * @return 解析器数量
     */
    size_t GetParserCount() const;
    
    /**
     * @brief 获取当前待处理数据数量
     * @return 待处理数据数量
     */
    size_t GetPendingCount() const;
    
    /**
     * @brief 获取处理器的分析器实例
     * @return 分析器实例
     */
    std::shared_ptr<analyzer::LogAnalyzer> GetAnalyzer();
    
private:
    // 处理线程函数
    void ProcessThreadFunc();
    
    // 处理单条日志数据
    void ProcessLogData(const LogData& data);
    
    // 解析日志数据
    analyzer::LogRecord ParseLogData(const LogData& data);
    
    // 存档原始日志数据
    void ArchiveLogData(const LogData& data, const analyzer::LogRecord& record);
    
    // 解压日志数据（如果已压缩）
    std::string DecompressLogData(const LogData& data);
    
    // 初始化TCP服务器进行网络日志接收
    bool InitializeTcpServer();
    
    // TCP消息处理回调
    void HandleTcpMessage(uint64_t connectionId, const std::string& message, muduo::Timestamp timestamp);
    
    // TCP连接处理回调
    void HandleTcpConnection(uint64_t connectionId, const std::string& clientAddr, bool connected);
    
    // 配置
    ProcessorConfig config_;
    
    // 解析器
    std::vector<std::shared_ptr<LogParser>> parsers_;
    mutable std::mutex parsersMutex_;
    
    // 统计信息
    size_t dataCount_{0};
    
    // 待处理的日志数据队列
    std::queue<LogData> pendingData_;
    mutable std::mutex dataMutex_;
    
    // 分析器
    std::shared_ptr<analyzer::LogAnalyzer> analyzer_;
    
    // 存储
    std::shared_ptr<storage::RedisStorage> redisStorage_;
    std::shared_ptr<storage::MySQLStorage> mysqlStorage_;
    
    // 网络服务器
    std::unique_ptr<network::TcpServer> tcpServer_;
    std::unordered_map<uint64_t, std::string> connections_;
    mutable std::mutex connectionsMutex_;
    
    // 线程池
    std::unique_ptr<common::ThreadPool> threadPool_;
    
    // 处理线程
    std::thread processThread_;
    std::atomic<bool> running_{false};
    
    // 回调函数
    ProcessCallback processCallback_;
    std::mutex callbackMutex_;
};

} // namespace processor
} // namespace xumj

#endif // XUMJ_PROCESSOR_LOG_PROCESSOR_H 