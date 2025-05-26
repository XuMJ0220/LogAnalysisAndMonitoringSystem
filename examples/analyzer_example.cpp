#include "xumj/analyzer/log_analyzer.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <nlohmann/json.hpp>

using namespace xumj;
using namespace xumj::analyzer;
using json = nlohmann::json;

// 分析结果回调函数
void AnalysisCallback(const std::string& recordId, 
                     const std::unordered_map<std::string, std::string>& results) {
    std::cout << "分析结果 - 记录ID: " << recordId << std::endl;
    for (const auto& [key, value] : results) {
        std::cout << "  " << key << ": " << value << std::endl;
    }
    std::cout << std::endl;
}

// 打印性能指标
void PrintMetrics(const AnalyzerMetrics& metrics) {
    std::cout << "\n性能指标统计：" << std::endl;
    std::cout << "总处理记录数: " << metrics.totalRecords << std::endl;
    std::cout << "错误记录数: " << metrics.errorRecords << std::endl;
    std::cout << "总处理时间(微秒): " << metrics.totalProcessTime << std::endl;
    std::cout << "峰值内存使用(字节): " << metrics.peakMemoryUsage << std::endl;
    
    std::cout << "\n规则性能指标：" << std::endl;
    for (const auto& [ruleName, ruleMetrics] : metrics.ruleMetrics) {
        std::cout << "规则: " << ruleName << std::endl;
        std::cout << "  匹配次数: " << ruleMetrics.matchCount << std::endl;
        std::cout << "  处理时间(微秒): " << ruleMetrics.processTime << std::endl;
        std::cout << "  错误次数: " << ruleMetrics.errorCount << std::endl;
        std::cout << "  最后匹配时间: " 
                  << std::chrono::system_clock::to_time_t(
                         std::chrono::system_clock::now()) << std::endl;
    }
}

int main() {
    try {
        // 创建分析器配置
        AnalyzerConfig config;
        config.threadPoolSize = 4;
        config.analyzeInterval = std::chrono::seconds(1);
        config.batchSize = 100;
        config.storeResults = true;
        config.enableMetrics = true;
        config.maxRetries = 3;
        config.ruleTimeout = std::chrono::milliseconds(1000);

        // Redis配置
        json redisConfig = {
            {"host", "localhost"},
            {"port", 6379},
            {"db", 0},
            {"password", "123465"}
        };
        config.redisConfigJson = redisConfig.dump();

        // MySQL配置
        json mysqlConfig = {
            {"host", "localhost"},
            {"port", 3306},
            {"user", "root"},
            {"password", "ytfhqqkso1"},
            {"database", "log_analysis"}
        };
        config.mysqlConfigJson = mysqlConfig.dump();

        // 创建分析器
        LogAnalyzer analyzer(config);

        // 创建规则配置
        RuleConfig errorRuleConfig;
        errorRuleConfig.priority = 100;
        errorRuleConfig.group = "error";
        errorRuleConfig.enabled = true;
        errorRuleConfig.maxRetries = 3;
        errorRuleConfig.timeout = std::chrono::milliseconds(500);

        RuleConfig securityRuleConfig;
        securityRuleConfig.priority = 50;
        securityRuleConfig.group = "security";
        securityRuleConfig.enabled = true;
        securityRuleConfig.maxRetries = 2;
        securityRuleConfig.timeout = std::chrono::milliseconds(1000);

        // 添加正则表达式规则
        std::vector<std::string> fieldNames = {"error_type", "error_message"};
        auto regexRule = std::make_shared<RegexAnalysisRule>(
            "error_rule",
            "error: (\\w+): (.*)",
            fieldNames
        );
        regexRule->SetConfig(errorRuleConfig);
        analyzer.AddRule(regexRule);

        // 添加关键字规则
        std::vector<std::string> keywords = {"security", "warning", "alert", "unauthorized"};
        auto keywordRule = std::make_shared<KeywordAnalysisRule>(
            "security_rule",
            keywords,
            true
        );
        keywordRule->SetConfig(securityRuleConfig);
        analyzer.AddRule(keywordRule);

        // 设置分析回调
        analyzer.SetAnalysisCallback(AnalysisCallback);

        // 启动分析器
        if (!analyzer.Start()) {
            std::cerr << "启动分析器失败" << std::endl;
            return 1;
        }

        // 创建一些测试日志记录
        std::vector<LogRecord> testRecords = {
            {
                "1",
                "2024-03-20 10:00:00",
                "ERROR",
                "app1",
                "error: DatabaseError: Connection failed",
                {}
            },
            {
                "2",
                "2024-03-20 10:01:00",
                "WARN",
                "app2",
                "security warning: unauthorized access attempt",
                {}
            },
            {
                "3",
                "2024-03-20 10:02:00",
                "INFO",
                "app1",
                "System started successfully",
                {}
            }
        };

        // 提交日志记录进行分析
        size_t submittedCount = analyzer.SubmitRecords(testRecords);
        std::cout << "已提交 " << submittedCount << " 条日志记录进行分析" << std::endl;

        // 等待分析完成
        std::this_thread::sleep_for(std::chrono::seconds(2));

        // 打印规则组信息
        std::cout << "\n规则组信息：" << std::endl;
        for (const auto& group : analyzer.GetRuleGroups()) {
            std::cout << "组: " << group << std::endl;
            auto rules = analyzer.GetRulesByGroup(group);
            for (const auto& rule : rules) {
                std::cout << "  规则: " << rule->GetName() 
                         << " (优先级: " << rule->GetConfig().priority << ")" << std::endl;
            }
        }

        // 打印性能指标
        PrintMetrics(analyzer.GetMetrics());

        // 测试规则组启用/禁用
        std::cout << "\n测试规则组启用/禁用：" << std::endl;
        analyzer.DisableGroup("security");
        std::cout << "已禁用security组" << std::endl;

        // 提交新的测试记录
        LogRecord newRecord = {
            "4",
            "2024-03-20 10:03:00",
            "WARN",
            "app2",
            "security alert: suspicious activity detected",
            {}
        };
        analyzer.SubmitRecord(newRecord);

        // 等待分析完成
        std::this_thread::sleep_for(std::chrono::seconds(1));

        // 重新启用security组
        analyzer.EnableGroup("security");
        std::cout << "已重新启用security组" << std::endl;

        // 停止分析器
        analyzer.Stop();

        return 0;
    } catch (const std::exception& e) {
        std::cerr << "发生错误: " << e.what() << std::endl;
        return 1;
    }
} 