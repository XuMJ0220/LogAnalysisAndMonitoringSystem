#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "xumj/processor/log_processor.h"

using namespace xumj::processor;

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

// 自定义JSON日志解析器
class CustomJsonLogParser : public JsonLogParser {
public:
    CustomJsonLogParser(const std::string& name) {
        name_ = name;
    }
    
    std::string GetType() const override {
        return name_;
    }

private:
    std::string name_;
};

int main() {
    std::cout << "开始日志处理器示例程序..." << std::endl;
    
    // 创建处理器配置
    LogProcessorConfig config;
    config.workerThreads = 2;
    config.queueSize = 100;
    config.debug = true;
    
    // 创建日志处理器
    LogProcessor processor(config);
    
    // 添加日志解析器
    processor.AddLogParser(std::make_shared<CustomJsonLogParser>("JsonLog"));
    
    // 获取分析器实例并添加分析规则
    auto analyzer = processor.GetAnalyzer();
    if (analyzer) {
        // 1. 错误状态规则
        analyzer->AddRule(std::make_shared<xumj::analyzer::RegexAnalysisRule>(
            "ErrorStatus",
            "状态：(失败|错误|异常)",
            std::vector<std::string>{"status"}
        ));
        
        // 2. 性能监控规则
        analyzer->AddRule(std::make_shared<xumj::analyzer::RegexAnalysisRule>(
            "Performance",
            "duration\":(\\d+)",
            std::vector<std::string>{"duration"}
        ));
        
        // 3. 关键字规则
        analyzer->AddRule(std::make_shared<xumj::analyzer::KeywordAnalysisRule>(
            "ErrorKeywords",
            std::vector<std::string>{"错误", "异常", "失败", "超时", "拒绝"},
            true
        ));
        
        // 设置分析完成回调
        analyzer->SetAnalysisCallback(OnAnalysisComplete);
    }
    
    // 启动处理器
    if (!processor.Start()) {
        std::cerr << "启动处理器失败" << std::endl;
        return 1;
    }
    
    std::cout << "处理器已启动" << std::endl;
    
    // 创建并提交示例日志数据
    std::vector<LogData> dataList;
    for (int i = 0; i < 15; ++i) {
        dataList.push_back(CreateSampleLogData(i));
    }
    
    // 单独提交日志数据
    for (const auto& data : dataList) {
        bool submitted = processor.SubmitLogData(data);
        if (submitted) {
            std::cout << "提交了日志数据：ID = " << data.id << std::endl;
        }
    }
    
    // 等待处理和分析完成
    std::cout << "等待处理完成..." << std::endl;
    
    // 循环检查并等待处理完成
    for (int i = 0; i < 10; ++i) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        size_t pendingCount = processor.GetPendingCount();
        std::cout << "剩余待处理数据数量：" << pendingCount << std::endl;
        
        if (pendingCount == 0) {
            break;
        }
    }
    
    // 再等待一段时间，确保分析完成
    std::cout << "等待分析完成..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // 停止处理器
    processor.Stop();
    std::cout << "处理器已停止" << std::endl;
    
    std::cout << "日志处理器示例程序结束" << std::endl;
    return 0;
} 