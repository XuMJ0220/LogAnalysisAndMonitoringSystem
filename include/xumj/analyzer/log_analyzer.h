#ifndef XUMJ_ANALYZER_LOG_ANALYZER_H
#define XUMJ_ANALYZER_LOG_ANALYZER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include "xumj/storage/redis_storage.h"
#include "xumj/storage/mysql_storage.h"
#include "xumj/common/thread_pool.h"

namespace xumj {
namespace analyzer {

/**
 * @struct LogRecord
 * @brief 日志记录结构，包含解析后的日志信息
 */
struct LogRecord {
    std::string id;                ///< 日志ID
    std::string timestamp;         ///< 时间戳
    std::string level;             ///< 日志级别
    std::string source;            ///< 日志来源
    std::string message;           ///< 日志消息
    std::unordered_map<std::string, std::string> fields;  ///< 解析出的字段
};

/**
 * @class AnalysisRule
 * @brief 日志分析规则接口
 */
class AnalysisRule {
public:
    virtual ~AnalysisRule() = default;
    
    /**
     * @brief 分析日志记录
     * @param record 日志记录
     * @return 分析结果（键值对形式）
     */
    virtual std::unordered_map<std::string, std::string> Analyze(const LogRecord& record) const = 0;
    
    /**
     * @brief 获取规则名称
     * @return 规则名称
     */
    virtual std::string GetName() const = 0;
};

/**
 * @class RegexAnalysisRule
 * @brief 基于正则表达式的日志分析规则
 */
class RegexAnalysisRule : public AnalysisRule {
public:
    /**
     * @brief 构造函数
     * @param name 规则名称
     * @param pattern 正则表达式模式
     * @param fieldNames 匹配组对应的字段名
     */
    RegexAnalysisRule(const std::string& name, 
                      const std::string& pattern,
                      const std::vector<std::string>& fieldNames);
    
    /**
     * @brief 分析日志记录
     * @param record 日志记录
     * @return 分析结果（键值对形式）
     */
    std::unordered_map<std::string, std::string> Analyze(const LogRecord& record) const override;
    
    /**
     * @brief 获取规则名称
     * @return 规则名称
     */
    std::string GetName() const override;
    
private:
    std::string name_;               ///< 规则名称
    std::string pattern_;            ///< 正则表达式模式
    std::vector<std::string> fieldNames_;  ///< 匹配组对应的字段名
    // 实际实现中可能需要预编译的正则表达式对象
};

/**
 * @class KeywordAnalysisRule
 * @brief 基于关键字的日志分析规则
 */
class KeywordAnalysisRule : public AnalysisRule {
public:
    /**
     * @brief 构造函数
     * @param name 规则名称
     * @param keywords 关键字列表
     * @param scoring 是否进行打分
     */
    KeywordAnalysisRule(const std::string& name,
                        const std::vector<std::string>& keywords,
                        bool scoring = false);
    
    /**
     * @brief 分析日志记录
     * @param record 日志记录
     * @return 分析结果（键值对形式）
     */
    std::unordered_map<std::string, std::string> Analyze(const LogRecord& record) const override;
    
    /**
     * @brief 获取规则名称
     * @return 规则名称
     */
    std::string GetName() const override;
    
private:
    std::string name_;               ///< 规则名称
    std::vector<std::string> keywords_;  ///< 关键字列表
    bool scoring_;                   ///< 是否进行打分
};

/**
 * @struct AnalyzerConfig
 * @brief 日志分析器配置
 */
struct AnalyzerConfig {
    size_t threadPoolSize{4};                 ///< 分析线程池大小
    std::chrono::seconds analyzeInterval{1};  ///< 分析间隔时间
    size_t batchSize{100};                    ///< 每批分析的日志数量
    bool storeResults{true};                  ///< 是否存储分析结果
    std::string redisConfigJson{};            ///< Redis配置JSON
    std::string mysqlConfigJson{};            ///< MySQL配置JSON
};

/**
 * @class LogAnalyzer
 * @brief 日志分析器，负责对日志进行解析、分析和处理
 */
class LogAnalyzer {
public:
    /**
     * @brief 分析完成回调函数类型
     * @param recordId 日志记录ID
     * @param results 分析结果
     */
    using AnalysisCallback = std::function<void(const std::string&, 
                                              const std::unordered_map<std::string, std::string>&)>;
    
    /**
     * @brief 构造函数
     * @param config 分析器配置
     */
    explicit LogAnalyzer(const AnalyzerConfig& config = AnalyzerConfig());
    
    /**
     * @brief 析构函数
     */
    ~LogAnalyzer();
    
    /**
     * @brief 初始化分析器
     * @param config 分析器配置
     * @return 是否成功初始化
     */
    bool Initialize(const AnalyzerConfig& config);
    
    /**
     * @brief 添加分析规则
     * @param rule 分析规则
     */
    void AddRule(std::shared_ptr<AnalysisRule> rule);
    
    /**
     * @brief 清除所有分析规则
     */
    void ClearRules();
    
    /**
     * @brief 提交日志记录进行分析
     * @param record 日志记录
     * @return 是否成功提交
     */
    bool SubmitRecord(const LogRecord& record);
    
    /**
     * @brief 批量提交日志记录进行分析
     * @param records 日志记录列表
     * @return 成功提交的记录数量
     */
    size_t SubmitRecords(const std::vector<LogRecord>& records);
    
    /**
     * @brief 设置分析完成回调函数
     * @param callback 回调函数
     */
    void SetAnalysisCallback(AnalysisCallback callback);
    
    /**
     * @brief 启动分析器
     * @return 是否成功启动
     */
    bool Start();
    
    /**
     * @brief 停止分析器
     */
    void Stop();
    
    /**
     * @brief 获取分析器是否正在运行
     * @return 是否正在运行
     */
    bool IsRunning() const;
    
    /**
     * @brief 获取当前规则数量
     * @return 规则数量
     */
    size_t GetRuleCount() const;
    
    /**
     * @brief 获取当前待处理记录数量
     * @return 待处理记录数量
     */
    size_t GetPendingCount() const;
    
private:
    // 分析线程函数
    void AnalyzeThreadFunc();
    
    // 处理单条日志记录
    void ProcessRecord(const LogRecord& record);
    
    // 存储分析结果到Redis
    void StoreResultToRedis(const std::string& recordId, 
                           const std::unordered_map<std::string, std::string>& results);
    
    // 存储分析结果到MySQL
    void StoreResultToMySQL(const std::string& recordId,
                           const std::unordered_map<std::string, std::string>& results);
    
    // 配置
    AnalyzerConfig config_;
    
    // 分析规则
    std::vector<std::shared_ptr<AnalysisRule>> rules_;
    mutable std::mutex rulesMutex_;
    
    // 待处理的日志记录队列
    std::vector<LogRecord> pendingRecords_;
    mutable std::mutex recordsMutex_;
    
    // 线程池
    std::unique_ptr<common::ThreadPool> threadPool_;
    
    // 存储
    std::shared_ptr<storage::RedisStorage> redisStorage_;
    std::shared_ptr<storage::MySQLStorage> mysqlStorage_;
    
    // 分析线程
    std::thread analyzeThread_;
    std::atomic<bool> running_{false};
    
    // 回调函数
    AnalysisCallback analysisCallback_;
    std::mutex callbackMutex_;
};

} // namespace analyzer
} // namespace xumj

#endif // XUMJ_ANALYZER_LOG_ANALYZER_H 