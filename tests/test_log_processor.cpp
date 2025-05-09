#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include "xumj/processor/log_processor.h"
#include "xumj/analyzer/log_analyzer.h"

using namespace xumj::processor;
using namespace xumj::analyzer;
using ::testing::Return;
using ::testing::_;
using ::testing::AtLeast;
using ::testing::NiceMock;

// 模拟日志解析器
class MockLogParser : public LogParser {
public:
    MOCK_CONST_METHOD1(Parse, LogRecord(const LogData&));
    MOCK_CONST_METHOD0(GetName, std::string());
    MOCK_CONST_METHOD1(CanParse, bool(const LogData&));
};

// 测试日志处理器的基本功能
TEST(LogProcessorTest_BasicFunctionality_Test, ProcessorInitialization) {
    // 创建处理器配置
    ProcessorConfig config;
    config.threadPoolSize = 2;
    config.batchSize = 10;
    config.maxQueueSize = 100;
    // 使用duration_cast将毫秒转换为秒
    config.processInterval = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(100));
    
    // 创建日志处理器
    LogProcessor processor(config);
    
    // 创建模拟解析器
    auto mockParser = std::make_shared<MockLogParser>();
    
    // 设置模拟解析器行为
    EXPECT_CALL(*mockParser, GetName())
        .WillRepeatedly(Return("MockParser"));
    
    EXPECT_CALL(*mockParser, CanParse(_))
        .WillRepeatedly(Return(true));
    
    EXPECT_CALL(*mockParser, Parse(_))
        .WillRepeatedly([](const LogData& data) {
            LogRecord record;
            record.id = data.id;
            record.timestamp = std::chrono::system_clock::to_time_t(data.timestamp);
            record.level = "INFO";
            record.message = data.data;
            record.source = data.source;
            return record;
        });
    
    // 添加解析器
    processor.AddParser(mockParser);
    
    // 设置处理回调
    bool callbackCalled = false;
    std::string processedLogId;
    
    processor.SetProcessCallback([&callbackCalled, &processedLogId](const std::string& id, bool success) {
        callbackCalled = true;
        processedLogId = id;
        EXPECT_TRUE(success);
    });
    
    // 启动处理器
    EXPECT_TRUE(processor.Start());
    
    // 提交日志数据
    LogData logData;
    logData.id = "test-log-123";
    logData.data = "This is a test log message";
    logData.source = "test-client";
    logData.timestamp = std::chrono::system_clock::now();
    
    EXPECT_TRUE(processor.SubmitLogData(logData));
    
    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    
    // 验证回调是否被调用
    EXPECT_TRUE(callbackCalled);
    EXPECT_EQ(processedLogId, "test-log-123");
    
    // 停止处理器
    processor.Stop();
}

// 测试处理大量日志
TEST(LogProcessorTest_ProcessManyLogs_Test, BatchProcessing) {
    // 创建处理器配置
    ProcessorConfig config;
    config.threadPoolSize = 4;
    config.batchSize = 50;
    config.maxQueueSize = 1000;
    // 使用duration_cast将毫秒转换为秒
    config.processInterval = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(10));
    
    // 创建日志处理器
    LogProcessor processor(config);
    
    // 创建简单解析器
    class SimpleParser : public LogParser {
    public:
        LogRecord Parse(const LogData& data) const override {
            LogRecord record;
            record.id = data.id;
            record.timestamp = std::chrono::system_clock::to_time_t(data.timestamp);
            record.level = "INFO";
            record.message = data.data;
            record.source = data.source;
            return record;
        }
        
        std::string GetName() const override {
            return "SimpleParser";
        }
        
        bool CanParse(const LogData&) const override { return true; }
    };
    
    // 添加解析器
    processor.AddParser(std::make_shared<SimpleParser>());
    
    // 设置处理回调
    std::mutex mutex;
    std::vector<std::string> processedEntries;
    
    processor.SetProcessCallback([&processedEntries, &mutex](const std::string& id, bool) {
        std::lock_guard<std::mutex> lock(mutex);
        processedEntries.push_back(id);
    });
    
    // 启动处理器
    EXPECT_TRUE(processor.Start());
    
    // 生成并提交多个日志
    const int logCount = 200;
    std::vector<std::string> submittedIds;
    
    for (int i = 0; i < logCount; ++i) {
        LogData logData;
        logData.id = "log-" + std::to_string(i);
        logData.data = "Test log message " + std::to_string(i);
        logData.source = "test-client";
        logData.timestamp = std::chrono::system_clock::now();
        
        submittedIds.push_back(logData.id);
        EXPECT_TRUE(processor.SubmitLogData(logData));
    }
    
    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 停止处理器
    processor.Stop();
    
    // 验证所有日志是否都被处理
    EXPECT_EQ(processedEntries.size(), static_cast<size_t>(logCount));
    
    // 验证处理的日志ID是否与提交的一致
    std::sort(processedEntries.begin(), processedEntries.end());
    std::sort(submittedIds.begin(), submittedIds.end());
    EXPECT_EQ(processedEntries, submittedIds);
}

// 测试解析器选择
TEST(LogProcessorTest_ParserSelection_Test, SelectCorrectParser) {
    // 创建处理器配置
    ProcessorConfig config;
    config.threadPoolSize = 2;
    config.batchSize = 10;
    // 使用duration_cast将毫秒转换为秒
    config.processInterval = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(100));
    
    // 创建日志处理器
    LogProcessor processor(config);
    
    // 创建模拟解析器
    auto jsonParser = std::make_shared<MockLogParser>();
    auto xmlParser = std::make_shared<MockLogParser>();
    auto textParser = std::make_shared<MockLogParser>();
    
    // 设置JSON解析器
    EXPECT_CALL(*jsonParser, GetName())
        .WillRepeatedly(Return("JsonParser"));
    
    EXPECT_CALL(*jsonParser, CanParse(_))
        .WillRepeatedly([](const LogData& data) {
            return data.data.find('{') != std::string::npos;
        });
    
    EXPECT_CALL(*jsonParser, Parse(_))
        .WillRepeatedly([](const LogData& data) {
            LogRecord record;
            record.id = data.id;
            record.level = "INFO";
            record.message = "JSON: " + data.data;
            return record;
        });
    
    // 设置XML解析器
    EXPECT_CALL(*xmlParser, GetName())
        .WillRepeatedly(Return("XmlParser"));
    
    EXPECT_CALL(*xmlParser, CanParse(_))
        .WillRepeatedly([](const LogData& data) {
            return data.data.find('<') != std::string::npos;
        });
    
    EXPECT_CALL(*xmlParser, Parse(_))
        .WillRepeatedly([](const LogData& data) {
            LogRecord record;
            record.id = data.id;
            record.level = "DEBUG";
            record.message = "XML: " + data.data;
            return record;
        });
    
    // 设置文本解析器
    EXPECT_CALL(*textParser, GetName())
        .WillRepeatedly(Return("TextParser"));
    
    EXPECT_CALL(*textParser, CanParse(_))
        .WillRepeatedly(Return(true));
    
    EXPECT_CALL(*textParser, Parse(_))
        .WillRepeatedly([](const LogData& data) {
            LogRecord record;
            record.id = data.id;
            record.level = "TRACE";
            record.message = "TEXT: " + data.data;
            return record;
        });
    
    // 添加解析器（顺序很重要）
    processor.AddParser(jsonParser);
    processor.AddParser(xmlParser);
    processor.AddParser(textParser);
    
    // 设置处理回调
    std::mutex mutex;
    std::unordered_map<std::string, LogRecord> processedRecords;
    
    processor.SetProcessCallback([](const std::string&, bool) {
        // 这里不需要处理回调内容
    });
    
    // 启动处理器
    EXPECT_TRUE(processor.Start());
    
    // 提交不同格式的日志
    LogData jsonLog;
    jsonLog.id = "json-log";
    jsonLog.data = "{\"message\":\"This is JSON\"}";
    jsonLog.timestamp = std::chrono::system_clock::now();
    
    LogData xmlLog;
    xmlLog.id = "xml-log";
    xmlLog.data = "<log><message>This is XML</message></log>";
    xmlLog.timestamp = std::chrono::system_clock::now();
    
    LogData textLog;
    textLog.id = "text-log";
    textLog.data = "This is plain text";
    textLog.timestamp = std::chrono::system_clock::now();
    
    // 提交日志
    processor.SubmitLogData(jsonLog);
    processor.SubmitLogData(xmlLog);
    processor.SubmitLogData(textLog);
    
    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(300));
    
    // 停止处理器
    processor.Stop();
}

// 测试配置更新
TEST(LogProcessorTest_ConfigUpdate_Test, UpdateConfiguration) {
    // 创建初始配置
    ProcessorConfig config;
    config.threadPoolSize = 2;
    config.batchSize = 10;
    config.maxQueueSize = 100;
    // 使用duration_cast将毫秒转换为秒
    config.processInterval = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(100));
    
    // 创建日志处理器
    LogProcessor processor(config);
    
    // 添加解析器
    auto mockParser = std::make_shared<MockLogParser>();
    EXPECT_CALL(*mockParser, GetName())
        .WillRepeatedly(Return("MockParser"));
    EXPECT_CALL(*mockParser, CanParse(_))
        .WillRepeatedly(Return(true));
    
    processor.AddParser(mockParser);
    
    // 启动处理器
    EXPECT_TRUE(processor.Start());
    
    // 创建新配置
    ProcessorConfig newConfig;
    newConfig.threadPoolSize = 4;
    newConfig.batchSize = 20;
    newConfig.maxQueueSize = 200;
    // 使用duration_cast将毫秒转换为秒
    newConfig.processInterval = std::chrono::duration_cast<std::chrono::seconds>(std::chrono::milliseconds(50));
    
    // 更新配置
    EXPECT_TRUE(processor.Initialize(newConfig));
    
    // 停止处理器
    processor.Stop();
} 