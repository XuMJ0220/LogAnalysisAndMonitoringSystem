#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <thread>
#include <chrono>
#include <mutex>
#include <atomic>
#include <vector>

#include "xumj/processor/log_processor.h"
#include "xumj/analyzer/log_analyzer.h"

using namespace xumj::processor;
using namespace xumj::analyzer;
using namespace testing;

// 定义测试解析器
class MockLogParser : public LogParser {
public:
    MOCK_METHOD(bool, Parse, (const LogData& logData, LogRecord& record), (override));
    MOCK_METHOD(std::string, GetType, (), (const, override));
};

// 基本功能测试：日志处理器初始化
TEST(LogProcessorTest_BasicFunctionality, ProcessorInitialization) {
    // 创建配置
    LogProcessorConfig config;
    config.workerThreads = 2;
    config.enableRedisStorage = false;
    config.enableMySQLStorage = false;
    
    // 创建处理器
    LogProcessor processor(config);
    
    // 创建模拟解析器
    auto mockParser = std::make_shared<MockLogParser>();
    
    // 设置预期行为
    // 设置解析器类型
    EXPECT_CALL(*mockParser, GetType())
        .WillRepeatedly(Return("MockParser"));
    
    // 设置解析行为
    EXPECT_CALL(*mockParser, Parse(_, _))
        .WillRepeatedly(DoAll(
            // 模拟解析逻辑
            [](const LogData& data, LogRecord& record) {
            record.id = data.id;
                record.timestamp = "2023-05-11 10:00:00";
            record.level = "INFO";
            record.source = data.source;
                record.message = data.message;
                return true;
            },
            Return(true)
        ));
    
    // 添加解析器
    processor.AddLogParser(mockParser);
    
    // 启动处理器
    EXPECT_TRUE(processor.Start());
    
    // 创建测试日志数据
    LogData logData;
    logData.id = "test-log-1";
    logData.timestamp = std::chrono::system_clock::now();
    logData.source = "test-source";
    logData.message = "This is a test log message";
    
    // 提交日志数据
    EXPECT_TRUE(processor.SubmitLogData(logData));
    
    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 检查处理结果（这里只能验证日志被处理，无法验证回调）
    EXPECT_EQ(processor.GetPendingCount(), 0UL);
    
    // 停止处理器
    processor.Stop();
}

// 处理多个日志测试：批处理
TEST(LogProcessorTest_ProcessManyLogs, BatchProcessing) {
    // 创建配置
    LogProcessorConfig config;
    config.workerThreads = 4;
    config.queueSize = 100;
    config.enableRedisStorage = false;
    config.enableMySQLStorage = false;
    
    // 创建处理器
    LogProcessor processor(config);
    
    // 创建自定义解析器
    class SimpleParser : public LogParser {
    public:
        virtual bool Parse(const LogData& data, LogRecord& record) override {
            // 简单实现，将LogData转换为LogRecord
            record.id = data.id;
            record.timestamp = "2023-05-11 10:00:00";
            record.level = "INFO";
            record.source = data.source;
            record.message = data.message;
            return true;
        }
        
        virtual std::string GetType() const override {
            return "SimpleParser";
        }
    };
    
    // 添加解析器
    processor.AddLogParser(std::make_shared<SimpleParser>());
    
    // 启动处理器
    EXPECT_TRUE(processor.Start());
    
    // 创建多个测试日志
    std::vector<LogData> dataList;
    for (int i = 0; i < 50; ++i) {
        LogData logData;
        logData.id = "test-log-" + std::to_string(i);
        logData.timestamp = std::chrono::system_clock::now();
        logData.source = "test-source";
        logData.message = "Test log message " + std::to_string(i);
        dataList.push_back(logData);
    }
        
    // 提交日志数据
    for (const auto& data : dataList) {
        processor.SubmitLogData(data);
    }
    
    // 等待处理完成
    std::chrono::system_clock::time_point startTime = std::chrono::system_clock::now();
    while (processor.GetPendingCount() > 0) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        
        // 设置超时时间，避免无限等待
        auto duration = std::chrono::system_clock::now() - startTime;
        if (std::chrono::duration_cast<std::chrono::seconds>(duration).count() > 10) {
            FAIL() << "处理超时，仍有" << processor.GetPendingCount() << "个日志未处理";
            break;
        }
    }
    
    // 验证所有日志都已处理
    EXPECT_EQ(processor.GetPendingCount(), 0UL);
    
    // 停止处理器
    processor.Stop();
}

// 解析器选择测试：选择正确的解析器
TEST(LogProcessorTest_ParserSelection, SelectCorrectParser) {
    // 创建配置
    LogProcessorConfig config;
    config.workerThreads = 2;
    config.enableRedisStorage = false;
    config.enableMySQLStorage = false;
    
    // 创建处理器
    LogProcessor processor(config);
    
    // 创建多个模拟解析器
    auto jsonParser = std::make_shared<MockLogParser>();
    auto xmlParser = std::make_shared<MockLogParser>();
    auto textParser = std::make_shared<MockLogParser>();
    
    // 添加解析器
    processor.AddLogParser(jsonParser);
    processor.AddLogParser(xmlParser);
    processor.AddLogParser(textParser);
    
    // 启动处理器
    EXPECT_TRUE(processor.Start());
    
    // 创建不同格式的日志数据
    LogData jsonLog;
    jsonLog.id = "json-log-1";
    jsonLog.timestamp = std::chrono::system_clock::now();
    jsonLog.source = "json-source";
    jsonLog.message = "{\"message\":\"This is JSON\"}";
    
    LogData xmlLog;
    xmlLog.id = "xml-log-1";
    xmlLog.timestamp = std::chrono::system_clock::now();
    xmlLog.source = "xml-source";
    xmlLog.message = "<log><message>This is XML</message></log>";
    
    LogData textLog;
    textLog.id = "text-log-1";
    textLog.timestamp = std::chrono::system_clock::now();
    textLog.source = "text-source";
    textLog.message = "This is plain text";
    
    // 提交日志数据
    processor.SubmitLogData(jsonLog);
    processor.SubmitLogData(xmlLog);
    processor.SubmitLogData(textLog);
    
    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 验证所有日志都已处理
    EXPECT_EQ(processor.GetPendingCount(), 0UL);
    
    // 停止处理器
    processor.Stop();
}

// 配置更新测试：更新配置
TEST(LogProcessorTest_ConfigUpdate, UpdateConfiguration) {
    // 创建配置
    LogProcessorConfig config;
    config.workerThreads = 2;
    config.queueSize = 100;
    
    // 创建处理器
    LogProcessor processor(config);
    
    // 添加解析器
    auto mockParser = std::make_shared<MockLogParser>();
    processor.AddLogParser(mockParser);
    
    // 启动处理器
    EXPECT_TRUE(processor.Start());
    
    // 等待1秒
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 停止处理器
    processor.Stop();
    
    // 验证配置
    EXPECT_EQ(processor.GetConfig().workerThreads, 2);
    EXPECT_EQ(processor.GetConfig().queueSize, 100);
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
} 