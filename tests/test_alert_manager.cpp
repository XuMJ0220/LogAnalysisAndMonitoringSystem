#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <string>
#include <vector>
#include <memory>
#include <chrono>
#include <thread>
#include "xumj/alert/alert_manager.h"

using namespace xumj::alert;
using namespace xumj::analyzer;
using ::testing::_;
using ::testing::Return;
using ::testing::SaveArg;

// 模拟通知渠道
class MockNotificationChannel : public NotificationChannel {
public:
    // 使用旧版本的MOCK_METHOD宏
    MOCK_METHOD1(SendAlert, bool(const Alert&));
    
    // 实现纯虚函数
    std::string GetName() const override { return name_; }
    std::string GetType() const override { return type_; }
    
    MockNotificationChannel(const std::string& name, const std::string& type) 
        : name_(name), type_(type) {}
    
    std::string name_;
    std::string type_;
};

// 测试告警规则
TEST(AlertManagerTest, AlertRules) {
    // 创建阈值规则
    auto thresholdRule = std::make_shared<ThresholdAlertRule>(
        "HighCpuRule",
        "CPU使用率过高",
        "cpu_usage",
        80.0,
        ">=",
        AlertLevel::WARNING
    );
    
    // 创建关键字规则
    std::vector<std::string> keywords = {"error", "failure", "critical"};
    auto keywordRule = std::make_shared<KeywordAlertRule>(
        "ErrorKeywordRule",
        "包含错误关键字",
        "message",
        keywords,
        false,
        AlertLevel::ERROR
    );
    
    // 测试规则属性
    EXPECT_EQ(thresholdRule->GetName(), "HighCpuRule");
    EXPECT_EQ(thresholdRule->GetDescription(), "CPU使用率过高");
    
    EXPECT_EQ(keywordRule->GetName(), "ErrorKeywordRule");
    EXPECT_EQ(keywordRule->GetDescription(), "包含错误关键字");
    
    // 创建日志记录和分析结果
    LogRecord record1;
    record1.id = "log-1";
    record1.timestamp = "2023-01-01 10:00:00";
    record1.level = "WARNING";
    record1.source = "server1";
    record1.message = "CPU usage is high";
    
    std::unordered_map<std::string, std::string> results1;
    results1["cpu_usage"] = "85.0";
    
    LogRecord record2;
    record2.id = "log-2";
    record2.timestamp = "2023-01-01 10:05:00";
    record2.level = "ERROR";
    record2.source = "server2";
    record2.message = "Database connection failure";
    
    std::unordered_map<std::string, std::string> results2;
    results2["db_connection"] = "failed";
    
    // 测试规则检查
    EXPECT_TRUE(thresholdRule->Check(record1, results1));
    EXPECT_FALSE(thresholdRule->Check(record2, results2));
    
    EXPECT_FALSE(keywordRule->Check(record1, results1));
    EXPECT_TRUE(keywordRule->Check(record2, results2));
    
    // 测试生成告警
    Alert alert1 = thresholdRule->GenerateAlert(record1, results1);
    EXPECT_EQ(alert1.name, "HighCpuRule");
    EXPECT_EQ(alert1.level, AlertLevel::WARNING);
    EXPECT_EQ(alert1.source, "server1");
    EXPECT_EQ(alert1.status, AlertStatus::PENDING);
    EXPECT_EQ(alert1.relatedLogIds.size(), 1U);
    EXPECT_EQ(alert1.relatedLogIds[0], "log-1");
    
    Alert alert2 = keywordRule->GenerateAlert(record2, results2);
    EXPECT_EQ(alert2.name, "ErrorKeywordRule");
    EXPECT_EQ(alert2.level, AlertLevel::ERROR);
    EXPECT_EQ(alert2.source, "server2");
    EXPECT_EQ(alert2.status, AlertStatus::PENDING);
    EXPECT_EQ(alert2.relatedLogIds.size(), 1U);
    EXPECT_EQ(alert2.relatedLogIds[0], "log-2");
}

// 测试告警管理器
TEST(AlertManagerTest, AlertManagement) {
    // 创建告警管理器配置
    AlertManagerConfig config;
    config.threadPoolSize = 2;
    config.checkInterval = std::chrono::seconds(1);
    config.resendInterval = std::chrono::seconds(5);
    config.suppressDuplicates = true;
    
    // 创建告警管理器
    AlertManager manager(config);
    
    // 创建规则
    auto thresholdRule = std::make_shared<ThresholdAlertRule>(
        "HighCpuRule",
        "CPU使用率过高",
        "cpu_usage",
        80.0,
        ">=",
        AlertLevel::WARNING
    );
    
    std::vector<std::string> keywords = {"error", "failure", "critical"};
    auto keywordRule = std::make_shared<KeywordAlertRule>(
        "ErrorKeywordRule",
        "包含错误关键字",
        "message",
        keywords,
        false,
        AlertLevel::ERROR
    );
    
    // 添加规则
    manager.AddRule(thresholdRule);
    manager.AddRule(keywordRule);
    
    EXPECT_EQ(manager.GetRuleCount(), 2U);
    
    // 创建模拟通知渠道
    auto mockChannel = std::make_shared<MockNotificationChannel>("TestChannel", "MOCK");
    
    // 设置期望
    EXPECT_CALL(*mockChannel, SendAlert(_))
        .WillRepeatedly(Return(true));
    
    // 添加通知渠道
    manager.AddChannel(mockChannel);
    
    EXPECT_EQ(manager.GetChannelCount(), 1U);
    
    // 记录被调用的告警ID
    std::vector<std::string> alertIds;
    std::vector<AlertStatus> alertStatuses;
    
    // 设置告警回调
    manager.SetAlertCallback([&alertIds, &alertStatuses](const std::string& alertId, AlertStatus status) {
        alertIds.push_back(alertId);
        alertStatuses.push_back(status);
    });
    
    // 启动告警管理器
    ASSERT_TRUE(manager.Start());
    
    // 创建日志记录和分析结果
    LogRecord record;
    record.id = "log-test";
    record.timestamp = "2023-01-01 10:00:00";
    record.level = "WARNING";
    record.source = "server1";
    record.message = "CPU usage is high";
    
    std::unordered_map<std::string, std::string> results;
    results["cpu_usage"] = "85.0";
    
    // 触发告警检查
    auto triggerIds = manager.CheckAlerts(record, results);
    
    // 应该触发一个告警
    EXPECT_EQ(triggerIds.size(), 1U);
    
    // 等待处理
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 验证回调被调用
    EXPECT_FALSE(alertIds.empty());
    EXPECT_EQ(alertStatuses.back(), AlertStatus::ACTIVE);
    
    // 获取活跃告警
    auto activeAlerts = manager.GetActiveAlerts();
    EXPECT_EQ(activeAlerts.size(), 1U);
    
    // 解决告警
    manager.ResolveAlert(activeAlerts[0].id);
    
    // 等待处理
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 验证告警已解决
    activeAlerts = manager.GetActiveAlerts();
    EXPECT_EQ(activeAlerts.size(), 0U);
    
    // 停止告警管理器
    manager.Stop();
}

// 测试通知渠道
TEST(AlertManagerTest, NotificationChannels) {
    // 创建告警管理器配置
    AlertManagerConfig config;
    config.threadPoolSize = 2;
    config.checkInterval = std::chrono::seconds(1);
    
    // 创建告警管理器
    AlertManager manager(config);
    
    // 添加通知渠道
    auto emailChannel = std::make_shared<MockNotificationChannel>("EmailChannel", "EMAIL");
    auto smsChannel = std::make_shared<MockNotificationChannel>("SMSChannel", "SMS");
    auto webhookChannel = std::make_shared<MockNotificationChannel>("WebhookChannel", "WEBHOOK");
    
    manager.AddChannel(emailChannel);
    manager.AddChannel(smsChannel);
    manager.AddChannel(webhookChannel);
    
    // 验证通知渠道数量
    EXPECT_EQ(manager.GetChannelCount(), 3U);
    
    // 移除通知渠道
    manager.RemoveChannel("SMSChannel");
    
    EXPECT_EQ(manager.GetChannelCount(), 2U);
}

// 测试告警级别和状态转换
TEST(AlertManagerTest, AlertLevelAndStatusConversion) {
    // 级别转换
    EXPECT_EQ(AlertLevelToString(AlertLevel::INFO), "INFO");
    EXPECT_EQ(AlertLevelToString(AlertLevel::WARNING), "WARNING");
    EXPECT_EQ(AlertLevelToString(AlertLevel::ERROR), "ERROR");
    EXPECT_EQ(AlertLevelToString(AlertLevel::CRITICAL), "CRITICAL");
    
    EXPECT_EQ(AlertLevelFromString("INFO"), AlertLevel::INFO);
    EXPECT_EQ(AlertLevelFromString("WARNING"), AlertLevel::WARNING);
    EXPECT_EQ(AlertLevelFromString("ERROR"), AlertLevel::ERROR);
    EXPECT_EQ(AlertLevelFromString("CRITICAL"), AlertLevel::CRITICAL);
    EXPECT_EQ(AlertLevelFromString("UNKNOWN"), AlertLevel::INFO);  // 默认值
    
    // 状态转换
    EXPECT_EQ(AlertStatusToString(AlertStatus::PENDING), "PENDING");
    EXPECT_EQ(AlertStatusToString(AlertStatus::ACTIVE), "ACTIVE");
    EXPECT_EQ(AlertStatusToString(AlertStatus::RESOLVED), "RESOLVED");
    EXPECT_EQ(AlertStatusToString(AlertStatus::IGNORED), "IGNORED");
    
    EXPECT_EQ(AlertStatusFromString("PENDING"), AlertStatus::PENDING);
    EXPECT_EQ(AlertStatusFromString("ACTIVE"), AlertStatus::ACTIVE);
    EXPECT_EQ(AlertStatusFromString("RESOLVED"), AlertStatus::RESOLVED);
    EXPECT_EQ(AlertStatusFromString("IGNORED"), AlertStatus::IGNORED);
    EXPECT_EQ(AlertStatusFromString("UNKNOWN"), AlertStatus::PENDING);  // 默认值
} 