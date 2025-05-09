#include <iostream>
#include <string>
#include <vector>
#include <thread>
#include <chrono>
#include "xumj/alert/alert_manager.h"
#include "xumj/analyzer/log_analyzer.h"

using namespace xumj::alert;
using namespace xumj::analyzer;

// 创建示例日志记录
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
        record.message = "系统CPU使用率达到 " + std::to_string(70 + index % 30) + 
                       "%，服务器：server" + std::to_string(1 + index % 5);
    } else if (index % 3 == 1) {
        record.message = "数据库响应时间: " + std::to_string(200 + index * 10) + 
                       " ms，查询ID: Q" + std::to_string(index);
    } else {
        record.message = "用户登录失败，用户名: user" + std::to_string(index) + 
                       "，原因: 密码错误，IP: 192.168.1." + std::to_string(index % 256);
    }
    
    // 添加一些自定义字段
    record.fields["threadId"] = std::to_string(index % 10);
    record.fields["sessionId"] = "session-" + std::to_string(1000 + index);
    
    return record;
}

// 创建示例分析结果
std::unordered_map<std::string, std::string> CreateSampleResults(int index, const LogRecord& record) {
    std::unordered_map<std::string, std::string> results;
    
    // 添加基本字段
    results["record.id"] = record.id;
    results["record.timestamp"] = record.timestamp;
    results["record.level"] = record.level;
    results["record.source"] = record.source;
    
    // 根据消息内容添加不同的分析结果
    if (index % 3 == 0) {
        results["SystemResource.matched"] = "true";
        results["SystemResource.memory_usage"] = std::to_string(70 + index % 30);
        results["SystemResource.server_name"] = "server" + std::to_string(1 + index % 5);
    } else if (index % 3 == 1) {
        results["Performance.matched"] = "true";
        results["Performance.query_time"] = std::to_string(200 + index * 10);
        results["Performance.query_id"] = "Q" + std::to_string(index);
    } else {
        results["UserLogin.matched"] = "true";
        results["UserLogin.user_id"] = "user" + std::to_string(index);
        results["UserLogin.status"] = "failed";
        results["UserLogin.reason"] = "密码错误";
        results["UserLogin.ip_address"] = "192.168.1." + std::to_string(index % 256);
    }
    
    // 添加一些通用字段
    for (const auto& [key, value] : record.fields) {
        results["record.fields." + key] = value;
    }
    
    return results;
}

// 告警回调函数
void OnAlertStatusChanged(const std::string& alertId, AlertStatus status) {
    std::cout << "告警状态变更: ID = " << alertId 
              << ", 状态 = " << AlertStatusToString(status) << std::endl;
}

int main() {
    std::cout << "开始告警管理器示例程序..." << std::endl;
    
    // 创建告警管理器配置
    AlertManagerConfig config;
    config.threadPoolSize = 2;
    config.checkInterval = std::chrono::seconds(5);
    config.resendInterval = std::chrono::seconds(30);
    config.suppressDuplicates = true;
    
    // 创建告警管理器
    AlertManager alertManager(config);
    
    // 添加告警规则
    
    // 1. CPU使用率阈值规则
    alertManager.AddRule(std::make_shared<ThresholdAlertRule>(
        "HighCpuUsage",
        "CPU使用率过高",
        "SystemResource.memory_usage",
        80.0,
        ">=",
        AlertLevel::WARNING
    ));
    
    // 2. 数据库响应时间阈值规则
    alertManager.AddRule(std::make_shared<ThresholdAlertRule>(
        "SlowDatabaseQuery",
        "数据库查询响应慢",
        "Performance.query_time",
        500.0,
        ">=",
        AlertLevel::ERROR
    ));
    
    // 3. 用户登录失败关键字规则
    std::vector<std::string> keywords = {"登录失败", "密码错误", "账号锁定"};
    alertManager.AddRule(std::make_shared<KeywordAlertRule>(
        "UserLoginFailure",
        "用户登录失败",
        "message",
        keywords,
        false,
        AlertLevel::INFO
    ));
    
    // 添加通知渠道
    
    // 1. 邮件通知
    std::vector<std::string> recipients = {"admin@example.com", "support@example.com"};
    alertManager.AddChannel(std::make_shared<EmailNotificationChannel>(
        "Email",
        "smtp.example.com",
        25,
        "alerts@example.com",
        "password",
        "alerts@example.com",
        recipients
    ));
    
    // 2. Webhook通知
    std::unordered_map<std::string, std::string> headers;
    headers["Authorization"] = "Bearer token123";
    
    alertManager.AddChannel(std::make_shared<WebhookNotificationChannel>(
        "Webhook",
        "https://example.com/webhook",
        headers
    ));
    
    // 设置告警回调
    alertManager.SetAlertCallback(OnAlertStatusChanged);
    
    // 启动告警管理器
    if (!alertManager.Start()) {
        std::cerr << "启动告警管理器失败" << std::endl;
        return 1;
    }
    
    std::cout << "告警管理器已启动，规则数量: " << alertManager.GetRuleCount() 
              << ", 通知渠道数量: " << alertManager.GetChannelCount() << std::endl;
    
    // 创建示例日志记录和分析结果
    std::vector<LogRecord> records;
    std::vector<std::unordered_map<std::string, std::string>> resultsList;
    
    for (int i = 0; i < 10; ++i) {
        // 根据索引生成不同级别的日志
        std::string level;
        if (i % 5 == 0) {
            level = "ERROR";
        } else if (i % 5 == 1) {
            level = "WARNING";
        } else {
            level = "INFO";
        }
        
        auto record = CreateSampleLogRecord(i, level);
        auto results = CreateSampleResults(i, record);
        
        records.push_back(record);
        resultsList.push_back(results);
    }
    
    // 检查告警规则
    std::cout << "\n开始检查告警规则..." << std::endl;
    for (size_t i = 0; i < records.size(); ++i) {
        std::cout << "\n处理日志记录 #" << i << ": " << records[i].message << std::endl;
        
        auto alertIds = alertManager.CheckAlerts(records[i], resultsList[i]);
        
        if (!alertIds.empty()) {
            std::cout << "触发了 " << alertIds.size() << " 个告警: ";
            for (const auto& id : alertIds) {
                std::cout << id << " ";
            }
            std::cout << std::endl;
            
            // 如果是最后一条记录，解决这些告警
            if (i == records.size() - 1) {
                for (const auto& id : alertIds) {
                    alertManager.ResolveAlert(id, "示例程序自动解决");
                }
            }
        } else {
            std::cout << "没有触发告警" << std::endl;
        }
        
        // 暂停一下，让告警有时间处理
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    // 手动触发一个告警
    Alert manualAlert;
    manualAlert.name = "ManualAlert";
    manualAlert.description = "这是一个手动触发的告警";
    manualAlert.level = AlertLevel::CRITICAL;
    manualAlert.source = "示例程序";
    manualAlert.labels["type"] = "manual";
    manualAlert.annotations["summary"] = "手动触发告警示例";
    
    std::string manualAlertId = alertManager.TriggerAlert(manualAlert);
    std::cout << "\n手动触发告警: " << manualAlertId << std::endl;
    
    // 等待片刻
    std::cout << "\n等待告警处理..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(3));
    
    // 获取活跃告警
    auto activeAlerts = alertManager.GetActiveAlerts();
    std::cout << "\n当前活跃告警数量: " << activeAlerts.size() << std::endl;
    
    for (const auto& alert : activeAlerts) {
        std::cout << "告警: " << alert.id << ", 名称: " << alert.name 
                  << ", 级别: " << AlertLevelToString(alert.level) << std::endl;
    }
    
    // 忽略手动触发的告警
    if (!manualAlertId.empty()) {
        std::cout << "\n忽略手动触发的告警: " << manualAlertId << std::endl;
        alertManager.IgnoreAlert(manualAlertId, "示例程序忽略测试");
    }
    
    // 等待一段时间
    std::cout << "\n等待最终处理..." << std::endl;
    std::this_thread::sleep_for(std::chrono::seconds(2));
    
    // 停止告警管理器
    alertManager.Stop();
    std::cout << "\n告警管理器已停止" << std::endl;
    
    std::cout << "\n告警管理器示例程序结束" << std::endl;
    return 0;
} 