#ifndef XUMJ_COLLECTOR_LOG_COLLECTOR_H
#define XUMJ_COLLECTOR_LOG_COLLECTOR_H

#include <string>
#include <vector>
#include <memory>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>
#include "xumj/common/memory_pool.h"
#include "xumj/common/lock_free_queue.h"
#include "xumj/common/thread_pool.h"

namespace xumj {
namespace collector {

/*
 * @enum LogLevel
 * @brief 日志级别枚举，用于标识日志的重要性
 */
enum class LogLevel {
    TRACE,      // 跟踪级别，最详细的日志信息
    DEBUG,      // 调试级别，用于开发调试
    INFO,       // 信息级别，普通信息
    WARNING,    // 警告级别，潜在问题
    ERROR,      // 错误级别，影响功能但不致命
    CRITICAL    // 严重级别，致命错误
};

/*
 * @class LogEntry
 * @brief 表示一条日志记录
 */
class LogEntry {
public:
    /*
     * @brief 构造函数
     * @param content 日志内容
     * @param level 日志级别
     * @param timestamp 日志生成时间戳（默认为当前时间）
     */
    LogEntry(std::string content, LogLevel level, 
             std::chrono::system_clock::time_point timestamp = std::chrono::system_clock::now())
        : content_(std::move(content)), level_(level), timestamp_(timestamp) {}
    
    /*
     * @brief 拷贝构造函数
     */
    LogEntry(const LogEntry& other)
        : content_(other.content_),
          level_(other.level_),
          timestamp_(other.timestamp_) {}
    
    /*
     * @brief 移动构造函数
     */
    LogEntry(LogEntry&& other) noexcept
        : content_(std::move(other.content_)),
          level_(other.level_),
          timestamp_(other.timestamp_) {}
    
    /*
     * @brief 拷贝赋值运算符
     */
    LogEntry& operator=(const LogEntry& other) {
        if (this != &other) {
            content_ = other.content_;
            level_ = other.level_;
            timestamp_ = other.timestamp_;
        }
        return *this;
    }
    
    /*
     * @brief 移动赋值运算符
     */
    LogEntry& operator=(LogEntry&& other) noexcept {
        if (this != &other) {
            content_ = std::move(other.content_);
            level_ = other.level_;
            timestamp_ = other.timestamp_;
        }
        return *this;
    }
    
    /*
     * @brief 获取日志内容
     * @return 日志内容字符串
     */
    const std::string& GetContent() const { return content_; }
    
    /*
     * @brief 获取日志级别
     * @return 日志级别枚举值
     */
    LogLevel GetLevel() const { return level_; }
    
    /*
     * @brief 获取日志时间戳
     * @return 时间戳
     */
    std::chrono::system_clock::time_point GetTimestamp() const { return timestamp_; }
    
private:
    std::string content_;                           // 日志内容
    LogLevel level_;                                // 日志级别
    std::chrono::system_clock::time_point timestamp_; // 时间戳
};

/*
 * @class LogFilterInterface
 * @brief 日志过滤器接口，定义了过滤日志的方法
 */
class LogFilterInterface {
public:
    virtual ~LogFilterInterface() = default;
    
    /*
     * @brief 判断一条日志是否应该被过滤
     * @param entry 日志条目
     * @return 如果日志应该被过滤掉返回true，否则返回false
     */
    virtual bool ShouldFilter(const LogEntry& entry) const = 0;
};

/*
 * @class LevelFilter
 * @brief 基于日志级别的过滤器
 */
class LevelFilter : public LogFilterInterface {
public:
    /*
     * @brief 构造函数
     * @param minLevel 最低接受的日志级别
     */
    explicit LevelFilter(LogLevel minLevel) : minLevel_(minLevel) {}
    
    /**
     * @brief 判断日志是否应该被过滤（低于指定级别的日志会被过滤掉）
     * @param entry 日志条目
     * @return 如果日志级别低于指定级别返回true，否则返回false
     */
    bool ShouldFilter(const LogEntry& entry) const override {
        return entry.GetLevel() < minLevel_;
    }
    
private:
    LogLevel minLevel_; // 最低接受的日志级别
};

/*
 * @class KeywordFilter
 * @brief 基于关键字的过滤器
 */
class KeywordFilter : public LogFilterInterface {
public:
    /*
     * @brief 构造函数
     * @param keywords 要过滤的关键字集合
     * @param filterMode 过滤模式（true为包含任意关键字则过滤，false为不包含任意关键字则过滤包含了则不过滤）
     */
    KeywordFilter(std::vector<std::string> keywords, bool filterMode = true)
        : keywords_(std::move(keywords)), filterMode_(filterMode) {}
    
    /*
     * @brief 判断日志是否应该被过滤
     * @param entry 日志条目
     * @return 根据过滤模式和关键字匹配情况返回结果
     */
    bool ShouldFilter(const LogEntry& entry) const override {
        bool containsAnyKeyword = false;
        for (const auto& keyword : keywords_) {
            if (entry.GetContent().find(keyword) != std::string::npos) {
                containsAnyKeyword = true;
                break;
            }
        }
        
        return filterMode_ ? containsAnyKeyword : !containsAnyKeyword; 
    }
    
private:
    std::vector<std::string> keywords_; // 关键字集合
    bool filterMode_;                   // 过滤模式
};

/*
 * @struct CollectorConfig
 * @brief 日志收集器的配置参数
 */
struct CollectorConfig {
    std::string collectorId;                  // 收集器唯一标识
    std::string serverAddress;                // 日志服务器地址
    uint16_t serverPort{8080};                // 日志服务器端口
    size_t batchSize{100};                    // 批处理大小
    std::chrono::milliseconds flushInterval{1000}; // 强制刷新间隔
    size_t maxQueueSize{10000};               // 最大队列大小
    size_t threadPoolSize{2};                 // 工作线程数量
    size_t memoryPoolSize{1024};              // 内存池大小
    LogLevel minLevel{LogLevel::INFO};        // 最低收集的日志级别
    bool compressLogs{true};                  // 是否压缩日志
    bool enableRetry{true};                   // 是否启用重试机制
    uint32_t maxRetryCount{3};                // 最大重试次数
    std::chrono::milliseconds retryInterval{5000}; // 重试间隔
};

/*
 * @class LogCollector
 * @brief 高性能日志收集器，用于采集、处理和发送日志数据
 * 
 * 此类实现了一个高效的日志收集器，支持异步批量处理、内存池优化、
 * 自动重试和多种过滤机制。它采集应用程序产生的日志，并将其发送到中央日志服务。
 */
class LogCollector {
public:
    /*
     * @brief 默认构造函数
     */
    LogCollector();
    
    /*
     * @brief 带配置的构造函数
     * @param config 收集器配置
     */
    explicit LogCollector(const CollectorConfig& config);
    
    /*
     * @brief 析构函数
     */
    ~LogCollector();
    
    /*
     * @brief 初始化收集器
     * @param config 收集器配置
     * @return 初始化是否成功
     */
    bool Initialize(const CollectorConfig& config);
    
    /*
     * @brief 提交单条日志
     * @param logContent 日志内容
     * @param level 日志级别
     * @return 提交是否成功
     */
    bool SubmitLog(const std::string& logContent, LogLevel level = LogLevel::INFO);
    
    /*
     * @brief 批量提交日志
     * @param logContents 日志内容集合
     * @param level 日志级别
     * @return 提交是否成功
     */
    bool SubmitLogs(const std::vector<std::string>& logContents, LogLevel level = LogLevel::INFO);
    
    /*
     * @brief 添加日志过滤器
     * @param filter 过滤器共享指针
     */
    void AddFilter(std::shared_ptr<LogFilterInterface> filter);
    
    /*
     * @brief 移除所有过滤器
     */
    void ClearFilters();
    
    /*
     * @brief 强制刷新日志（立即发送当前缓存的所有日志）
     */
    void Flush();
    
    /*
     * @brief 关闭收集器
     */
    void Shutdown();
    
    /*
     * @brief 获取已收集但未发送的日志数量
     * @return 未发送的日志数量
     */
    size_t GetPendingCount() const;
    
    /*
     * @brief 设置发送回调函数
     * @param callback 当日志被成功发送时的回调函数
     */
    void SetSendCallback(std::function<void(size_t)> callback);
    
    /*
     * @brief 设置错误回调函数
     * @param callback 当发送失败时的回调函数
     */
    void SetErrorCallback(std::function<void(const std::string&)> callback);
    
private:
    CollectorConfig config_;                                     // 收集器配置
    std::atomic<bool> isActive_;                                // 收集器是否活动
    common::LockFreeQueue<LogEntry> logQueue_;                   // 日志队列
    std::unique_ptr<common::ThreadPool> threadPool_;             // 工作线程池
    std::unique_ptr<common::MemoryPool> memoryPool_;             // 内存池
    std::vector<std::shared_ptr<LogFilterInterface>> filters_;   // 过滤器列表
    mutable std::mutex filtersMutex_;                           // 过滤器互斥锁
    std::thread flushThread_;                                   // 定时刷新线程
    std::function<void(size_t)> sendCallback_;                   // 发送成功回调
    std::function<void(const std::string&)> errorCallback_;       // 错误回调
    
    /*
     * @brief 发送日志批次
     * @param logs 日志条目批次
     * @return 发送是否成功
     */
    bool SendLogBatch(std::vector<LogEntry>& logs);
    
    /*
     * @brief 定时刷新线程函数
     */
    void FlushThreadFunc();
    
    /*
     * @brief 应用过滤规则
     * @param entry 日志条目
     * @return 日志是否应该被过滤
     */
    bool ShouldFilterLog(const LogEntry& entry) const;
    
    /*
     * @brief 处理重试逻辑
     * @param logs 日志条目批次
     */
    void HandleRetry(const std::vector<LogEntry>& logs);
    
    /*
     * @brief 压缩日志内容
     * @param content 原始日志内容
     * @return 压缩后的日志内容
     */
    std::string CompressLogContent(const std::string& content);
};

} // namespace collector
} // namespace xumj

#endif // XUMJ_COLLECTOR_LOG_COLLECTOR_H 