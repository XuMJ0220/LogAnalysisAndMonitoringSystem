#include <gtest/gtest.h>
#include <string>
#include <memory>
#include <vector>
#include <unordered_map>
#include "xumj/analyzer/log_analyzer.h"

using namespace xumj::analyzer;

// 测试基本规则功能
TEST(AnalyzerRuleTest, BasicRuleFunctionality) {
    // 创建正则表达式规则
    auto regexRule = std::make_shared<RegexAnalysisRule>(
        "ErrorRegexRule",
        "error|exception|failed",
        std::vector<std::string>{"has_error"}
    );
    
    // 测试规则属性
    EXPECT_EQ(regexRule->GetName(), "ErrorRegexRule");
    
    // 创建日志记录
    LogRecord record1;
    record1.id = "log-1";
    record1.timestamp = "2023-01-01 10:00:00";
    record1.level = "ERROR";
    record1.source = "server1";
    record1.message = "Database connection error occurred";
    
    LogRecord record2;
    record2.id = "log-2";
    record2.timestamp = "2023-01-01 10:05:00";
    record2.level = "INFO";
    record2.source = "server2";
    record2.message = "Operation completed successfully";
    
    // 测试规则应用
    auto results1 = regexRule->Analyze(record1);
    EXPECT_TRUE(results1.size() > 0);
    auto it1 = results1.find("has_error");
    EXPECT_TRUE(it1 != results1.end());
    if (it1 != results1.end()) {
        EXPECT_EQ(it1->second, "true");
    }
    
    auto results2 = regexRule->Analyze(record2);
    EXPECT_TRUE(results2.size() > 0);
    auto it2 = results2.find("has_error");
    EXPECT_TRUE(it2 != results2.end());
    if (it2 != results2.end()) {
        EXPECT_EQ(it2->second, "false");
    }
}

// 测试关键字分析规则
TEST(AnalyzerRuleTest, KeywordAnalysisRule) {
    // 创建关键字分析规则
    auto keywordRule = std::make_shared<KeywordAnalysisRule>(
        "CPUUsageRule",
        std::vector<std::string>{"CPU", "usage", "detected"},
        true
    );
    
    // 测试规则属性
    EXPECT_EQ(keywordRule->GetName(), "CPUUsageRule");
    
    // 创建日志记录
    LogRecord record1;
    record1.id = "log-1";
    record1.timestamp = "2023-01-01 10:00:00";
    record1.level = "WARNING";
    record1.source = "server1";
    record1.message = "System monitor: CPU usage: 87.5% detected";
    
    LogRecord record2;
    record2.id = "log-2";
    record2.timestamp = "2023-01-01 10:05:00";
    record2.level = "INFO";
    record2.source = "server2";
    record2.message = "System stable, no issues detected";
    
    // 测试规则应用
    auto results1 = keywordRule->Analyze(record1);
    EXPECT_TRUE(results1.size() > 0);
    
    auto results2 = keywordRule->Analyze(record2);
    EXPECT_TRUE(results2.size() > 0);
}

// 测试日志分析器功能
TEST(AnalyzerRuleTest, LogAnalyzerFunctionality) {
    // 创建分析器配置
    AnalyzerConfig config;
    config.threadPoolSize = 2;
    config.batchSize = 10;
    config.storeResults = false;
    
    // 创建日志分析器
    LogAnalyzer analyzer(config);
    
    // 添加规则
    auto regexRule = std::make_shared<RegexAnalysisRule>(
        "ErrorRegexRule",
        "error|exception|failed",
        std::vector<std::string>{"has_error"}
    );
    
    auto keywordRule = std::make_shared<KeywordAnalysisRule>(
        "KeywordRule",
        std::vector<std::string>{"CPU", "memory", "disk"},
        true
    );
    
    analyzer.AddRule(regexRule);
    analyzer.AddRule(keywordRule);
    
    // 验证规则数量
    EXPECT_EQ(analyzer.GetRuleCount(), 2U);
    
    // 创建日志记录
    LogRecord record;
    record.id = "log-1";
    record.timestamp = "2023-01-01 10:00:00";
    record.level = "ERROR";
    record.source = "server1";
    record.message = "Database connection error occurred";
    
    // 设置回调函数
    bool callbackInvoked = false;
    std::string callbackRecordId;
    std::unordered_map<std::string, std::string> callbackResults;
    
    analyzer.SetAnalysisCallback([&](const std::string& recordId, 
                                   const std::unordered_map<std::string, std::string>& results) {
        callbackInvoked = true;
        callbackRecordId = recordId;
        callbackResults = results;
    });
    
    // 启动分析器
    EXPECT_TRUE(analyzer.Start());
    
    // 提交记录
    EXPECT_TRUE(analyzer.SubmitRecord(record));
    
    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
    
    // 停止分析器
    analyzer.Stop();
    
    // 验证回调是否被调用
    EXPECT_TRUE(callbackInvoked);
    EXPECT_EQ(callbackRecordId, "log-1");
    EXPECT_TRUE(callbackResults.size() > 0);
} 