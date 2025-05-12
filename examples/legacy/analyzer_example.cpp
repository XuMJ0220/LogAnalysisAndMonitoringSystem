#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "xumj/analyzer/log_analyzer.h"

using namespace xumj::analyzer;

// 示例日志记录
LogRecord CreateSampleLogRecord(int index, const std::string& level) {
    LogRecord record;
    record.id = "log-" + std::to_string(index);
    
    // 设置当前时间
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = *std::localtime(&time_t_now);
    char buffer[30];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_now);
    record.timestamp = buffer;
    
    record.level = level;
    record.source = "示例程序";
    
    // 根据索引生成不同的消息
    if (index % 3 == 0) {
        record.message = "用户登录成功，用户ID：" + std::to_string(100 + index) + 
                         "，IP地址：192.168.1." + std::to_string(index % 256);
    } else if (index % 3 == 1) {
        record.message = "数据库查询耗时：" + std::to_string(50 + index % 100) + 
                         " ms，查询ID：Q" + std::to_string(index);
    } else {
        record.message = "系统警告：内存使用率达到 " + std::to_string(60 + index % 30) + 
                         "%，服务器：server" + std::to_string(1 + index % 5);
    }
    
    // 添加一些自定义字段
    record.fields["thread_id"] = std::to_string(index % 10);
    record.fields["process_id"] = std::to_string(1000 + index);
    
    return record;
}

// 分析完成回调函数
void OnAnalysisComplete(const std::string& recordId, 
                      const std::unordered_map<std::string, std::string>& results) {
    std::cout << "分析完成：日志ID = " << recordId << std::endl;
    std::cout << "分析结果：" << std::endl;
    
    for (const auto& [key, value] : results) {
        std::cout << "  " << key << " = " << value << std::endl;
    }
    
    std::cout << "------------------------" << std::endl;
}

int main() {
    std::cout << "开始日志分析器示例程序..." << std::endl;
    
    // 创建分析器配置
    AnalyzerConfig config;
    config.threadPoolSize = 2;
    config.analyzeInterval = std::chrono::seconds(1);
    config.batchSize = 10;
    config.storeResults = false;  // 示例中不存储结果
    
    // 创建日志分析器
    LogAnalyzer analyzer(config);
    
    // 添加分析规则
    
    // 1. 用户登录规则（正则表达式规则）
    analyzer.AddRule(std::make_shared<RegexAnalysisRule>(
        "UserLogin",
        "用户登录成功，用户ID：(\\d+)，IP地址：([\\d\\.]+)",
        std::vector<std::string>{"user_id", "ip_address"}
    ));
    
    // 2. 性能监控规则（正则表达式规则）
    analyzer.AddRule(std::make_shared<RegexAnalysisRule>(
        "Performance",
        "数据库查询耗时：(\\d+) ms，查询ID：(Q\\d+)",
        std::vector<std::string>{"query_time", "query_id"}
    ));
    
    // 3. 系统资源规则（正则表达式规则）
    analyzer.AddRule(std::make_shared<RegexAnalysisRule>(
        "SystemResource",
        "系统警告：内存使用率达到 (\\d+)%，服务器：(server\\d+)",
        std::vector<std::string>{"memory_usage", "server_name"}
    ));
    
    // 4. 错误关键字规则（关键字规则）
    analyzer.AddRule(std::make_shared<KeywordAnalysisRule>(
        "ErrorKeywords",
        std::vector<std::string>{"错误", "异常", "失败", "超时", "拒绝"},
        true
    ));
    
    // 设置分析完成回调
    analyzer.SetAnalysisCallback(OnAnalysisComplete);
    
    // 启动分析器
    if (!analyzer.Start()) {
        std::cerr << "启动分析器失败" << std::endl;
        return 1;
    }
    
    std::cout << "分析器已启动，规则数量：" << analyzer.GetRuleCount() << std::endl;
    
    // 创建并提交示例日志记录
    std::vector<LogRecord> records;
    for (int i = 0; i < 20; ++i) {
        // 根据索引生成不同级别的日志
        std::string level;
        if (i % 5 == 0) {
            level = "ERROR";
        } else if (i % 5 == 1) {
            level = "WARNING";
        } else {
            level = "INFO";
        }
        
        records.push_back(CreateSampleLogRecord(i, level));
    }
    
    // 添加一些包含错误关键字的日志
    LogRecord errorRecord;
    errorRecord.id = "error-log-1";
    errorRecord.timestamp = "2023-07-15 10:30:45";
    errorRecord.level = "ERROR";
    errorRecord.source = "示例程序";
    errorRecord.message = "发生异常：数据库连接超时，操作被拒绝";
    records.push_back(errorRecord);
    
    // 批量提交日志记录
    size_t submittedCount = analyzer.SubmitRecords(records);
    std::cout << "提交了 " << submittedCount << " 条日志记录进行分析" << std::endl;
    
    // 等待分析完成
    std::cout << "等待分析完成..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(5));
    
    // 检查待处理记录数量
    std::cout << "剩余待处理记录数量：" << analyzer.GetPendingCount() << std::endl;
    
    // 如果还有待处理记录，再等待一段时间
    if (analyzer.GetPendingCount() > 0) {
        std::cout << "继续等待..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }
    
    // 停止分析器
    analyzer.Stop();
    std::cout << "分析器已停止" << std::endl;
    
    std::cout << "日志分析器示例程序结束" << std::endl;
    return 0;
} 