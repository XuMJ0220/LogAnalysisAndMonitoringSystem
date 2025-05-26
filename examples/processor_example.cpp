#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include <fstream>
#include <regex>  // 添加正则表达式头文件
#include <iomanip>  // 添加iomanip头文件
#include "xumj/processor/log_processor.h"
#include "xumj/analyzer/log_analyzer.h" // 明确包含analyzer头文件
#include "xumj/storage/storage_factory.h"  // 明确包含storage头文件

using namespace xumj::processor;
using namespace xumj::analyzer; // 添加analyzer命名空间
// 使用analyzer内的类型
using LogRecord = xumj::analyzer::LogRecord;
using namespace xumj::storage;

// 创建示例日志数据
LogData CreateSampleLogData(int index) {
    LogData data;
    data.id = "data-" + std::to_string(index);
    data.timestamp = std::chrono::system_clock::now();
    data.source = "example-client";
    
    // 根据索引生成不同格式的日志数据
    if (index % 3 == 0) {
        // 生成类似于nginx访问日志格式
        data.message = "192.168.1." + std::to_string(index % 256) + 
                   " - user" + std::to_string(index % 100) + 
                   " [15/Jul/2023:10:30:" + std::to_string(index % 60) + 
                   " +0800] \"GET /api/resource/" + std::to_string(index) + 
                   " HTTP/1.1\" 200 " + std::to_string(1024 + index % 1000) + 
                   " \"http://example.com/referer\" \"Mozilla/5.0\"";
    } else if (index % 3 == 1) {
        // 生成JSON格式日志
        data.message = "{\"timestamp\":\"2023-07-15T10:30:" + std::to_string(index % 60) + 
                   "\",\"level\":\"" + (index % 5 == 0 ? "ERROR" : "INFO") + 
                   "\",\"source\":\"TestApp\",\"message\":\"操作" + 
                   std::to_string(index) + "完成\",\"duration\":" + 
                   std::to_string(50 + index % 100) + 
                   ",\"user_id\":" + std::to_string(1000 + index) + "}";
        data.metadata["is_json"] = "true";  // 标记为JSON格式
    } else {
        // 生成简单的文本日志
        data.message = "[2023-07-15 10:30:" + std::to_string(index % 60) + 
                   "] [" + (index % 5 == 0 ? "ERROR" : "INFO") + 
                   "] 进程" + std::to_string(index) + 
                   "执行任务" + std::to_string(index * 10) + 
                   "，状态：" + (index % 7 == 0 ? "失败" : "成功");
    }
    
    // 添加一些元数据
    data.metadata["client_version"] = "1.0." + std::to_string(index % 10);
    data.metadata["session_id"] = "session-" + std::to_string(index / 5);
    
    return data;
}

// 创建故障日志数据
LogData CreateErrorLogData(int index) {
    LogData data;
    data.id = "error-" + std::to_string(index);
    data.timestamp = std::chrono::system_clock::now();
    data.source = "error-generator";
    
    // 生成各种可能导致解析错误的日志格式
    switch (index % 4) {
        case 0:
            // 不完整JSON
            data.message = "{\"level\":\"ERROR\",\"message\":\"系统崩溃\",\"reason";
            data.metadata["is_json"] = "true";
            break;
        case 1:
            // 空消息
            data.message = "";
            break;
        case 2:
            // 超大日志
            for (int i = 0; i < 1000; i++) {
                data.message += "很长的日志内容重复多次导致日志过大 ";
            }
            break;
        case 3:
            // 包含特殊字符的JSON
            data.message = "{\"level\":\"ERROR\",\"message\":\"错误包含特殊字符：\n\t\r\b\",\"code\":500}";
            data.metadata["is_json"] = "true";
            break;
    }
    
    return data;
}

// 分析完成回调函数
void OnAnalysisComplete(const std::string& logId, const std::unordered_map<std::string, std::string>& results) {
    std::cout << "分析完成：日志ID = " << logId << std::endl;
    std::cout << "分析结果：" << std::endl;
    
    // 只显示部分重要结果，避免输出过多
    size_t shownCount = 0;
    for (const auto& [key, value] : results) {
        if (key.find("matched") != std::string::npos || 
            key.find("rule") != std::string::npos || 
            shownCount < 3) {
            std::cout << "  " << key << " = " << value << std::endl;
            shownCount++;
        }
    }
    
    if (results.size() > shownCount) {
        std::cout << "  ... 以及其他 " << (results.size() - shownCount) << " 个结果" << std::endl;
    }
    
    std::cout << "------------------------" << std::endl;
}

// 自定义日志解析器 - 解析纯文本日志
class TextLogParser : public LogParser {
public:
    TextLogParser() = default;
    
    std::string GetType() const override {
        return "TextParser";
    }
    
    bool Parse(const LogData& logData, xumj::analyzer::LogRecord& record) override {
        // 检查元数据是否指示JSON格式
        if (logData.metadata.count("is_json") > 0 && logData.metadata.at("is_json") == "true") {
            return false;  // 跳过JSON格式，让JsonLogParser处理
        }
        
        if (config_.debug) {
            std::cout << "TextLogParser: 尝试解析文本日志，ID=" << logData.id << std::endl;
        }
        
        record.id = logData.id;
        record.timestamp = TimestampToString(logData.timestamp);
        record.source = logData.source;
        record.message = logData.message;
        record.level = "INFO"; // 默认级别
        
        // 尝试提取时间、级别等信息
        const std::string& content = logData.message;
        
        // 常见的文本日志格式：[time] [level] message
        std::regex timePattern(R"(\[([^\]]+)\])");
        std::regex levelPattern(R"(\[(INFO|DEBUG|WARN|ERROR|FATAL)\])");
        
        std::smatch timeMatch, levelMatch;
        
        // 提取时间
        if (std::regex_search(content, timeMatch, timePattern) && timeMatch.size() > 1) {
            record.timestamp = timeMatch[1].str();
            if (config_.debug) {
                std::cout << "  从文本中提取时间戳: " << record.timestamp << std::endl;
            }
        }
        
        // 提取级别
        if (std::regex_search(content, levelMatch, levelPattern) && levelMatch.size() > 1) {
            record.level = levelMatch[1].str();
            if (config_.debug) {
                std::cout << "  从文本中提取级别: " << record.level << std::endl;
            }
        }
        
        // 尝试提取消息部分（假设消息在最后一个]之后）
        size_t lastBracket = content.rfind(']');
        if (lastBracket != std::string::npos && lastBracket + 1 < content.length()) {
            record.message = content.substr(lastBracket + 1);
            // 删除开头的空格
            record.message.erase(0, record.message.find_first_not_of(" \t\r\n"));
            if (config_.debug) {
                std::cout << "  从文本中提取消息: " << record.message << std::endl;
            }
        }
        
        // 检查日志中的关键词，添加到字段
        std::vector<std::string> keywords = {"错误", "异常", "失败", "成功", "完成", "执行"};
        for (const auto& keyword : keywords) {
            if (content.find(keyword) != std::string::npos) {
                record.fields["text.contains." + keyword] = "true";
                if (config_.debug) {
                    std::cout << "  发现关键词: " << keyword << std::endl;
                }
            }
        }
        
        // 提取IP地址（如果有）
        std::regex ipPattern(R"((\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}))");
        std::smatch ipMatch;
        if (std::regex_search(content, ipMatch, ipPattern) && ipMatch.size() > 1) {
            record.fields["text.client_ip"] = ipMatch[1].str();
            if (config_.debug) {
                std::cout << "  提取IP地址: " << record.fields["text.client_ip"] << std::endl;
            }
        }
        
        if (config_.debug) {
            std::cout << "TextLogParser: 解析成功" << std::endl;
        }
        return true;
    }
};

// 将日志记录写入文件的示例类
class FileLogWriter {
public:
    FileLogWriter(const std::string& filename) : filename_(filename) {
        file_.open(filename, std::ios::out | std::ios::app);
        if (!file_.is_open()) {
            std::cerr << "无法打开日志文件：" << filename << std::endl;
        }
    }
    
    ~FileLogWriter() {
        if (file_.is_open()) {
            file_.close();
        }
    }
    
    void WriteLog(const xumj::analyzer::LogRecord& record) {
        if (!file_.is_open()) return;
        
        // 简单地将日志记录格式化为CSV格式
        file_ << record.id << ","
              << record.timestamp << ","
              << record.level << ","
              << record.source << ","
              << "\"" << EscapeString(record.message) << "\""
              << std::endl;
    }

private:
    std::string filename_;
    std::ofstream file_;
    
    // 转义字符串中的双引号，用于CSV格式
    std::string EscapeString(const std::string& str) {
        std::string result = str;
        size_t pos = 0;
        while ((pos = result.find("\"", pos)) != std::string::npos) {
            result.replace(pos, 1, "\"\"");
            pos += 2;
        }
        return result;
    }
};

int main() {
    // 配置处理器
    LogProcessorConfig config;
    config.debug = true;
    config.workerThreads = 4;
    config.queueSize = 1000;
    config.tcpPort = 8001;
    config.enableRedisStorage = true;
    config.enableMySQLStorage = true;  // 启用MySQL存储
    
    // 配置Redis连接信息
    config.redisConfig.host = "localhost";
    config.redisConfig.port = 6379;
    config.redisConfig.password = "123465";
    config.redisConfig.database = 0;
    config.redisConfig.poolSize = 10;
    
    // 配置MySQL连接信息
    config.mysqlConfig.host = "localhost";
    config.mysqlConfig.port = 3306;
    config.mysqlConfig.username = "root";
    config.mysqlConfig.password = "ytfhqqkso1";
    config.mysqlConfig.database = "log_analysis";
    config.mysqlConfig.poolSize = 10;
    
    // 启用指标收集
    config.enableMetrics = true;
    config.metricsOutputPath = "processor_metrics.log";
    config.metricsFlushInterval = 30;  // 每30秒刷新一次指标
    
    // 创建处理器
    LogProcessor processor(config);
    
    // 添加解析器
    auto jsonParser = std::make_shared<JsonLogParser>();
    jsonParser->SetConfig(config);
    processor.AddLogParser(jsonParser);
    
    auto textParser = std::make_shared<TextLogParser>();
    textParser->SetConfig(config);
    processor.AddLogParser(textParser);
    
    // 创建分析器
    AnalyzerConfig analyzerConfig;
    analyzerConfig.enableMetrics = true;
    analyzerConfig.threadPoolSize = 4;
    analyzerConfig.batchSize = 100;
    analyzerConfig.analyzeInterval = std::chrono::seconds(1);
    
    auto analyzer = std::make_shared<LogAnalyzer>(analyzerConfig);
    analyzer->SetAnalysisCallback(OnAnalysisComplete);
    processor.SetAnalyzer(analyzer);
    
    // 启动处理器
    if (!processor.Start()) {
        std::cerr << "启动处理器失败" << std::endl;
        return 1;
    }
    
    // 创建文件日志写入器
    FileLogWriter logWriter("processed_logs.csv");
    
    // 生成并提交测试日志
    std::cout << "开始生成测试日志..." << std::endl;
    
    for (int i = 0; i < 100; ++i) {
        // 生成正常日志
        auto logData = CreateSampleLogData(i);
        if (!processor.SubmitLogData(logData)) {
            std::cerr << "提交日志失败: " << logData.id << std::endl;
        }
        
        // 每10条日志生成一条错误日志
        if (i % 10 == 0) {
            auto errorData = CreateErrorLogData(i);
            if (!processor.SubmitLogData(errorData)) {
                std::cerr << "提交错误日志失败: " << errorData.id << std::endl;
            }
        }
        
        // 每20条日志打印一次指标
        if (i % 20 == 0) {
            const auto& metrics = processor.GetMetrics();
            std::cout << "\n=== 处理器指标 ===" << std::endl;
            std::cout << "总处理记录数: " << metrics.totalRecords << std::endl;
            std::cout << "错误记录数: " << metrics.errorRecords << std::endl;
            std::cout << "总处理时间(微秒): " << metrics.totalProcessTime << std::endl;
            
            std::cout << "\n解析器指标:" << std::endl;
            for (const auto& [name, parserMetrics] : metrics.parserMetrics) {
                std::cout << "解析器: " << name << std::endl;
                std::cout << "  成功次数: " << parserMetrics.successCount << std::endl;
                std::cout << "  失败次数: " << parserMetrics.failureCount << std::endl;
                if (parserMetrics.successCount + parserMetrics.failureCount > 0) {
                    double successRate = static_cast<double>(parserMetrics.successCount) / 
                        (parserMetrics.successCount + parserMetrics.failureCount) * 100;
                    std::cout << "  成功率: " << std::fixed << std::setprecision(2) << successRate << "%" << std::endl;
                }
            }
            std::cout << "==================\n" << std::endl;
        }
        
        // 短暂休眠，模拟真实场景
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    
    // 等待处理完成
    std::cout << "等待处理完成..." << std::endl;
    while (processor.GetPendingCount() > 0) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    // 导出最终指标
    processor.ExportMetrics();
    
    // 停止处理器
    processor.Stop();
    
    std::cout << "测试完成" << std::endl;
    return 0;
} 