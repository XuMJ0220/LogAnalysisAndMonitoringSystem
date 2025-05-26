#ifndef XUMJ_ANALYZER_ANALYZER_METRICS_H
#define XUMJ_ANALYZER_ANALYZER_METRICS_H

#include <atomic>
#include <chrono>
#include <unordered_map>
#include <string>
#include <mutex>

namespace xumj {
namespace analyzer {

/*
 * @struct RuleMetrics
 * @brief 规则性能指标
 */
struct RuleMetrics {
    std::atomic<uint64_t> matchCount{0};        // 匹配次数
    std::atomic<uint64_t> processTime{0};       // 处理时间（微秒）
    std::atomic<uint64_t> errorCount{0};        // 错误次数
    std::chrono::steady_clock::time_point lastMatchTime;  // 最后匹配时间
};

/*
 * @struct AnalyzerMetrics
 * @brief 分析器性能指标
 */
struct AnalyzerMetrics {
    std::atomic<uint64_t> totalRecords{0};      // 总处理记录数
    std::atomic<uint64_t> pendingRecords{0};    // 待处理记录数
    std::atomic<uint64_t> errorRecords{0};      // 错误记录数
    std::atomic<uint64_t> totalProcessTime{0};  // 总处理时间（微秒）
    std::atomic<uint64_t> peakMemoryUsage{0};   // 峰值内存使用（字节）
    
    // 规则指标
    std::unordered_map<std::string, RuleMetrics> ruleMetrics;
    std::mutex metricsMutex;
    
    // 重置所有指标
    void Reset() {
        totalRecords = 0;
        pendingRecords = 0;
        errorRecords = 0;
        totalProcessTime = 0;
        peakMemoryUsage = 0;
        
        std::lock_guard<std::mutex> lock(metricsMutex);
        ruleMetrics.clear();
    }
    
    // 获取规则指标
    RuleMetrics& GetRuleMetrics(const std::string& ruleName) {
        std::lock_guard<std::mutex> lock(metricsMutex);
        return ruleMetrics[ruleName];
    }
};

} // namespace analyzer
} // namespace xumj

#endif // XUMJ_ANALYZER_ANALYZER_METRICS_H 