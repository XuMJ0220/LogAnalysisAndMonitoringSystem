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
#include <regex>
#include "xumj/storage/redis_storage.h"
#include "xumj/storage/mysql_storage.h"
#include "xumj/common/thread_pool.h"
#include "xumj/analyzer/analyzer_metrics.h"

namespace xumj {
namespace analyzer {

/*
 * @struct LogRecord
 * @brief 日志记录结构，包含解析后的日志信息
 */
struct LogRecord {
    std::string id;                // 日志ID
    std::string timestamp;         // 时间戳
    std::string level;             // 日志级别
    std::string source;            // 日志来源
    std::string message;           // 日志消息
    std::unordered_map<std::string, std::string> fields;  // 解析出的字段
};

/*
 * @struct RuleConfig
 * @brief 规则配置
 */
struct RuleConfig {
    int priority{0};                    // 规则优先级（数字越大优先级越高）
    std::string group{"default"};       // 规则分组
    bool enabled{true};                 // 规则是否启用
    size_t maxRetries{3};              // 最大重试次数
    std::chrono::milliseconds timeout{1000};  // 规则超时时间
};

/*
 * @class AnalysisRule
 * @brief 日志分析规则接口
 */
class AnalysisRule {
public:
    virtual ~AnalysisRule() = default;
    
    /*
     * @brief 分析日志记录
     * @param record 日志记录
     * @return 分析结果（键值对形式）
     */
    virtual std::unordered_map<std::string, std::string> Analyze(const LogRecord& record) const = 0;
    
    /*
     * @brief 获取规则名称
     * @return 规则名称
     */
    virtual std::string GetName() const = 0;
    
    /*
     * @brief 获取规则配置
     * @return 规则配置
     */
    virtual const RuleConfig& GetConfig() const = 0;
    
    /*
     * @brief 设置规则配置
     * @param config 规则配置
     */
    virtual void SetConfig(const RuleConfig& config) = 0;
    
    /*
     * @brief 启用规则
     */
    virtual void Enable() = 0;
    
    /*
     * @brief 禁用规则
     */
    virtual void Disable() = 0;
    
    /*
     * @brief 检查规则是否启用
     * @return 是否启用
     */
    virtual bool IsEnabled() const = 0;
};

/*
 * @class RegexAnalysisRule
 * @brief 基于正则表达式的日志分析规则
 */
class RegexAnalysisRule : public AnalysisRule {
public:
    /*
     * @brief 构造函数
     * @param name 规则名称
     * @param pattern 正则表达式模式
     * @param fieldNames 匹配组对应的字段名
     */
    RegexAnalysisRule(const std::string& name, 
                      const std::string& pattern,
                      const std::vector<std::string>& fieldNames);
    
    /*
     * @brief 分析日志记录
     * @param record 日志记录
     * @return 分析结果（键值对形式）
     */
    std::unordered_map<std::string, std::string> Analyze(const LogRecord& record) const override;
    
    /*
     * @brief 获取规则名称
     * @return 规则名称
     */
    std::string GetName() const override;
    
    /*
     * @brief 获取规则配置
     * @return 规则配置
     */
    const RuleConfig& GetConfig() const override;
    
    /*
     * @brief 设置规则配置
     * @param config 规则配置
     */
    void SetConfig(const RuleConfig& config) override;
    
    /*
     * @brief 启用规则
     */
    void Enable() override;
    
    /*
     * @brief 禁用规则
     */
    void Disable() override;
    
    /*
     * @brief 检查规则是否启用
     * @return 是否启用
     */
    bool IsEnabled() const override;
    
private:
    std::string name_;               // 规则名称
    std::string pattern_;            // 正则表达式模式
    std::vector<std::string> fieldNames_;  // 匹配组对应的字段名
    RuleConfig config_;               // 规则配置
    std::unique_ptr<std::regex> regexPattern_;  // 正则表达式对象
};

/*
 * @class KeywordAnalysisRule
 * @brief 基于关键字的日志分析规则
 */
class KeywordAnalysisRule : public AnalysisRule {
public:
    /*
     * @brief 构造函数
     * @param name 规则名称
     * @param keywords 关键字列表
     * @param scoring 是否进行打分
     */
    KeywordAnalysisRule(const std::string& name,
                        const std::vector<std::string>& keywords,
                        bool scoring = false);
    
    /*
     * @brief 分析日志记录
     * @param record 日志记录
     * @return 分析结果（键值对形式）
     */
    std::unordered_map<std::string, std::string> Analyze(const LogRecord& record) const override;
    
    /*
     * @brief 获取规则名称
     * @return 规则名称
     */
    std::string GetName() const override;
    
    /*
     * @brief 获取规则配置
     * @return 规则配置
     */
    const RuleConfig& GetConfig() const override;
    
    /*
     * @brief 设置规则配置
     * @param config 规则配置
     */
    void SetConfig(const RuleConfig& config) override;
    
    /*
     * @brief 启用规则
     */
    void Enable() override;
    
    /*
     * @brief 禁用规则
     */
    void Disable() override;
    
    /*
     * @brief 检查规则是否启用
     * @return 是否启用
     */
    bool IsEnabled() const override;
    
private:
    std::string name_;               // 规则名称
    std::vector<std::string> keywords_;  // 关键字列表
    bool scoring_;                   // 是否进行打分
    RuleConfig config_;               // 规则配置
};

/*
 * @struct AnalyzerConfig
 * @brief 日志分析器配置
 */
struct AnalyzerConfig {
    size_t threadPoolSize{4};                 // 分析线程池大小
    std::chrono::seconds analyzeInterval{1};  // 分析间隔时间
    size_t batchSize{100};                    // 每批分析的日志数量
    bool storeResults{true};                  // 是否存储分析结果
    std::string redisConfigJson{};            // Redis配置JSON
    std::string mysqlConfigJson{};            // MySQL配置JSON
    bool enableMetrics{true};                 // 是否启用性能指标收集
    size_t maxRetries{3};                     // 最大重试次数
    std::chrono::milliseconds ruleTimeout{1000};  // 规则超时时间
};

/*
 * @class LogAnalyzer
 * @brief 日志分析器，负责对日志进行解析、分析和处理
 */
class LogAnalyzer {
public:
    /*
     * @brief 分析完成回调函数类型
     * @param recordId 日志记录ID
     * @param results 分析结果
     */
    using AnalysisCallback = std::function<void(const std::string&, 
                                              const std::unordered_map<std::string, std::string>&)>;
    
    /*
     * @brief 构造函数
     * @param config 分析器配置
     */
    explicit LogAnalyzer(const AnalyzerConfig& config = AnalyzerConfig());
    
    /*
     * @brief 析构函数
     */
    ~LogAnalyzer();
    
    /*
     * @brief 初始化分析器
     * @param config 分析器配置
     * @return 是否成功初始化
     */
    bool Initialize(const AnalyzerConfig& config);
    
    /*
     * @brief 添加分析规则
     * @param rule 分析规则
     */
    void AddRule(std::shared_ptr<AnalysisRule> rule);
    
    /*
     * @brief 清除所有分析规则
     */
    void ClearRules();
    
    /*
     * @brief 提交日志记录进行分析
     * @param record 日志记录
     * @return 是否成功提交
     */
    bool SubmitRecord(const LogRecord& record);
    
    /*
     * @brief 批量提交日志记录进行分析
     * @param records 日志记录列表
     * @return 成功提交的记录数量
     */
    size_t SubmitRecords(const std::vector<LogRecord>& records);
    
    /*
     * @brief 设置分析完成回调函数
     * @param callback 回调函数
     */
    void SetAnalysisCallback(AnalysisCallback callback);
    
    /*
     * @brief 启动分析器
     * @return 是否成功启动
     */
    bool Start();
    
    /*
     * @brief 停止分析器
     */
    void Stop();
    
    /*
     * @brief 获取分析器是否正在运行
     * @return 是否正在运行
     */
    bool IsRunning() const;
    
    /*
     * @brief 获取当前规则数量
     * @return 规则数量
     */
    size_t GetRuleCount() const;
    
    /*
     * @brief 获取当前待处理记录数量
     * @return 待处理记录数量
     */
    size_t GetPendingCount() const;
    
    /*
     * @brief 获取性能指标
     * @return 性能指标
     */
    const AnalyzerMetrics& GetMetrics() const;

    /*
     * @brief 重置性能指标
     */
    void ResetMetrics();
    
    /*
     * @brief 获取规则组列表
     * @return 规则组列表
     */
    std::vector<std::string> GetRuleGroups() const;
    
    /*
     * @brief 获取指定组的规则列表
     * @param group 规则组名
     * @return 规则列表
     */
    std::vector<std::shared_ptr<AnalysisRule>> GetRulesByGroup(const std::string& group) const;
    
    /**
     * @brief 启用指定组的所有规则
     * @param group 规则组名
     */
    void EnableGroup(const std::string& group);
    
    /*
     * @brief 禁用指定组的所有规则
     * @param group 规则组名
     */
    void DisableGroup(const std::string& group);
    
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
    
    AnalyzerMetrics metrics_;  // 性能指标
    std::unordered_map<std::string, std::vector<std::shared_ptr<AnalysisRule>>> ruleGroups_;  // 规则分组
    
    void UpdateMetrics(const std::string& ruleName, 
                      const std::chrono::microseconds& processTime,
                      bool hasError);
    
    void SortRulesByPriority();
};

} // namespace analyzer
} // namespace xumj

#endif // XUMJ_ANALYZER_LOG_ANALYZER_H 