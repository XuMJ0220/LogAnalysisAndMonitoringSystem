#ifndef XUMJ_ALERT_ALERT_MANAGER_H
#define XUMJ_ALERT_ALERT_MANAGER_H

#include <string>
#include <vector>
#include <memory>
#include <functional>
#include <unordered_map>
#include <mutex>
#include <thread>
#include <atomic>
#include <chrono>
#include <queue>
#include "xumj/analyzer/log_analyzer.h"
#include "xumj/storage/redis_storage.h"
#include "xumj/storage/mysql_storage.h"
#include "xumj/network/tcp_client.h"

namespace xumj {
namespace alert {

/*
 * @enum AlertLevel
 * @brief 告警级别枚举
 */
enum class AlertLevel {
    INFO,      // 信息
    WARNING,   // 警告
    ERROR,     // 错误
    CRITICAL   // 严重
};

/*
 * @enum AlertStatus
 * @brief 告警状态枚举
 */
enum class AlertStatus {
    PENDING,   // 待处理
    ACTIVE,    // 活跃
    RESOLVED,  // 已解决
    IGNORED    // 已忽略
};

/*
 * @struct Alert
 * @brief 告警结构，包含告警信息
 */
struct Alert {
    std::string id;                // 告警ID
    std::string name;              // 告警名称
    std::string description;       // 告警描述
    AlertLevel level;              // 告警级别
    AlertStatus status;            // 告警状态
    std::string source;            // 告警来源
    std::chrono::system_clock::time_point timestamp;  // 触发时间
    std::chrono::system_clock::time_point updateTime; // 更新时间
    std::unordered_map<std::string, std::string> labels;    // 标签
    std::unordered_map<std::string, std::string> annotations; // 注解
    std::vector<std::string> relatedLogIds;  // 相关日志ID
    int count{1};                  // 告警次数
};

/*
 * @class AlertRule
 * @brief 告警规则接口
 */
class AlertRule {
public:
    virtual ~AlertRule() = default;

    /*
     * @brief 检查是否触发告警
     * @param record 日志记录
     * @param results 分析结果
     * @return 是否触发告警
     */
    virtual bool Check(
        const analyzer::LogRecord& record,
        const std::unordered_map<std::string, std::string>& results) const = 0;
    
    /*
     * @brief 生成告警
     * @param record 日志记录
     * @param results 分析结果
     * @return 告警
     */
    virtual Alert GenerateAlert(
        const analyzer::LogRecord& record,
        const std::unordered_map<std::string, std::string>& results) const = 0;
    
    /*
     * @brief 获取规则名称
     * @return 规则名称
     */
    virtual std::string GetName() const = 0;
    
    /*
     * @brief 获取规则描述
     * @return 规则描述
     */
    virtual std::string GetDescription() const = 0;
};

/*
 * @class ThresholdAlertRule
 * @brief 基于阈值的告警规则
 */
class ThresholdAlertRule : public AlertRule {
public:
    /*
     * @brief 构造函数
     * @param name 规则名称
     * @param description 规则描述
     * @param field 检查的字段
     * @param threshold 阈值
     * @param compareType 比较类型（>,<,>=,<=,==,!=）
     * @param level 告警级别
     */
    ThresholdAlertRule(
        const std::string& name,
        const std::string& description,
        const std::string& field,
        double threshold,
        const std::string& compareType,
        AlertLevel level = AlertLevel::WARNING
    );
    
    /*
     * @brief 检查是否触发告警
     * @param record 日志记录
     * @param results 分析结果
     * @return 是否触发告警
     */
    bool Check(
        const analyzer::LogRecord& record,
        const std::unordered_map<std::string, std::string>& results) const override;
    
    /*
     * @brief 生成告警
     * @param record 日志记录
     * @param results 分析结果
     * @return 告警
     */
    Alert GenerateAlert(
        const analyzer::LogRecord& record,
        const std::unordered_map<std::string, std::string>& results) const override;
    
    /*
     * @brief 获取规则名称
     * @return 规则名称
     */
    std::string GetName() const override;
    
    /*
     * @brief 获取规则描述
     * @return 规则描述
     */
    std::string GetDescription() const override;
    
private:
    std::string name_;          // 规则名称
    std::string description_;   // 规则描述
    std::string field_;         // 检查的字段
    double threshold_;          // 阈值
    std::string compareType_;   // 比较类型
    AlertLevel level_;          // 告警级别
};

/*
 * @class KeywordAlertRule
 * @brief 基于关键字的告警规则
 */
class KeywordAlertRule : public AlertRule {
public:
    /*
     * @brief 构造函数
     * @param name 规则名称
     * @param description 规则描述
     * @param field 检查的字段
     * @param keywords 关键字列表
     * @param matchAll 是否匹配所有关键字
     * @param level 告警级别
     */
    KeywordAlertRule(
        const std::string& name,
        const std::string& description,
        const std::string& field,
        const std::vector<std::string>& keywords,
        bool matchAll = false,
        AlertLevel level = AlertLevel::WARNING
    );
    
    /*
     * @brief 检查是否触发告警
     * @param record 日志记录
     * @param results 分析结果
     * @return 是否触发告警
     */
    bool Check(
        const analyzer::LogRecord& record,
        const std::unordered_map<std::string, std::string>& results) const override;
    
    /*
     * @brief 生成告警
     * @param record 日志记录
     * @param results 分析结果
     * @return 告警
     */
    Alert GenerateAlert(
        const analyzer::LogRecord& record,
        const std::unordered_map<std::string, std::string>& results) const override;
    
    /*
     * @brief 获取规则名称
     * @return 规则名称
     */
    std::string GetName() const override;
    
    /*
     * @brief 获取规则描述
     * @return 规则描述
     */
    std::string GetDescription() const override;
    
private:
    std::string name_;          // 规则名称
    std::string description_;   // 规则描述
    std::string field_;         // 检查的字段
    std::vector<std::string> keywords_;  // 关键字列表
    bool matchAll_;             // 是否匹配所有关键字
    AlertLevel level_;          // 告警级别
};

/*
 * @class NotificationChannel
 * @brief 通知渠道接口
 */
class NotificationChannel {

public:
    virtual ~NotificationChannel() = default;
    
    /*
     * @brief 发送告警通知
     * @param alert 告警
     * @return 是否发送成功
     */
    virtual bool SendAlert(const Alert& alert) = 0;
    
    /*
     * @brief 获取渠道名称
     * @return 渠道名称
     */
    virtual std::string GetName() const = 0;
    
    /*
     * @brief 获取渠道类型
     * @return 渠道类型
     */
    virtual std::string GetType() const = 0;
};

/*
 * @class EmailNotificationChannel
 * @brief 邮件通知渠道
 */
class EmailNotificationChannel : public NotificationChannel {
public:
    /*
     * @brief 构造函数
     * @param name 渠道名称
     * @param smtpServer SMTP服务器
     * @param smtpPort SMTP端口
     * @param username 用户名
     * @param password 密码
     * @param from 发件人
     * @param to 收件人列表
     * @param useTLS 是否使用TLS
     */
    EmailNotificationChannel(
        const std::string& name,
        const std::string& smtpServer,
        int smtpPort,
        const std::string& username,
        const std::string& password,
        const std::string& from,
        const std::vector<std::string>& to,
        bool useTLS = true
    );
    
    /*
     * @brief 发送告警通知
     * @param alert 告警
     * @return 是否发送成功
     */
    bool SendAlert(const Alert& alert) override;
    
    /*
     * @brief 获取渠道名称
     * @return 渠道名称
     */
    std::string GetName() const override;
    
    /*
     * @brief 获取渠道类型
     * @return 渠道类型
     */
    std::string GetType() const override;
    
private:
    std::string name_;          // 渠道名称
    std::string smtpServer_;    // SMTP服务器
    int smtpPort_;              // SMTP端口
    std::string username_;      // 用户名
    std::string password_;      // 密码
    std::string from_;          // 发件人
    std::vector<std::string> to_;  // 收件人列表
    bool useTLS_;               // 是否使用TLS
};

/*
 * @class WebhookNotificationChannel
 * @brief Webhook通知渠道
 */
class WebhookNotificationChannel : public NotificationChannel {
public:
    /*
     * @brief 构造函数
     * @param name 渠道名称
     * @param url Webhook URL
     * @param headers HTTP头
     * @param timeout 超时时间（秒）
     */
    WebhookNotificationChannel(
        const std::string& name,
        const std::string& url,
        const std::unordered_map<std::string, std::string>& headers = {},
        int timeout = 5
    );
    
    /*
     * @brief 发送告警通知
     * @param alert 告警
     * @return 是否发送成功
     */
    bool SendAlert(const Alert& alert) override;
    
    /*
     * @brief 获取渠道名称
     * @return 渠道名称
     */
    std::string GetName() const override;
    
    /*
     * @brief 获取渠道类型
     * @return 渠道类型
     */
    std::string GetType() const override;
    
private:
    std::string name_;          ///< 渠道名称
    std::string url_;           ///< Webhook URL
    std::unordered_map<std::string, std::string> headers_;  ///< HTTP头
    int timeout_;               ///< 超时时间
};

/*
 * @struct AlertManagerConfig
 * @brief 告警管理器配置
 */
struct AlertManagerConfig {
    size_t threadPoolSize{4};                 ///< 处理线程池大小
    std::chrono::seconds checkInterval{10};   ///< 检查间隔时间
    std::chrono::seconds resendInterval{300}; ///< 重发间隔时间
    size_t batchSize{50};                     ///< 每批处理的告警数量
    std::string redisConfigJson{};            ///< Redis配置JSON
    std::string mysqlConfigJson{};            ///< MySQL配置JSON
    bool suppressDuplicates{true};            ///< 是否抑制重复告警
    std::chrono::seconds groupInterval{60};   ///< 告警分组间隔
};

/*
 * @class AlertManager
 * @brief 告警管理器，负责告警规则检查和通知发送
 */
class AlertManager {
public:
    /*
     * @brief 告警回调函数类型
     * @param alertId 告警ID
     * @param status 告警状态
     */
    using AlertCallback = std::function<void(const std::string&, AlertStatus)>;
    
    /*
     * @brief 构造函数
     * @param config 配置
     */
    explicit AlertManager(const AlertManagerConfig& config = AlertManagerConfig());
    
    /*
     * @brief 析构函数
     */
    ~AlertManager();
    
    /*
     * @brief 初始化告警管理器
     * @param config 配置
     * @return 是否成功初始化
     */
    bool Initialize(const AlertManagerConfig& config);
    
    /*
     * @brief 添加告警规则
     * @param rule 规则
     */
    void AddRule(std::shared_ptr<AlertRule> rule);
    
    /*
     * @brief 移除告警规则
     * @param ruleName 规则名称
     * @return 是否成功移除
     */
    bool RemoveRule(const std::string& ruleName);
    
    /*
     * @brief 清除所有规则
     */
    void ClearRules();
    
    /*
     * @brief 添加通知渠道
     * @param channel 通知渠道
     */
    void AddChannel(std::shared_ptr<NotificationChannel> channel);
    
    /*
     * @brief 移除通知渠道
     * @param channelName 渠道名称
     * @return 是否成功移除
     */
    bool RemoveChannel(const std::string& channelName);
    
    /*
     * @brief 清除所有通知渠道
     */
    void ClearChannels();
    
    /*
     * @brief 检查日志记录是否触发告警
     * @param record 日志记录
     * @param results 分析结果
     * @return 触发的告警ID
     */
    std::vector<std::string> CheckAlerts(
        const analyzer::LogRecord& record,
        const std::unordered_map<std::string, std::string>& results);
    
    /*
     * @brief 手动触发告警
     * @param alert 告警
     * @return 告警ID
     */
    std::string TriggerAlert(const Alert& alert);
    
    /*
     * @brief 解决告警
     * @param alertId 告警ID
     * @param comment 注释
     * @return 是否成功解决
     */
    bool ResolveAlert(const std::string& alertId, const std::string& comment = "");
    
    /*
     * @brief 忽略告警
     * @param alertId 告警ID
     * @param comment 注释
     * @return 是否成功忽略
     */
    bool IgnoreAlert(const std::string& alertId, const std::string& comment = "");
    
    /*
     * @brief 获取告警信息
     * @param alertId 告警ID
     * @return 告警
     */
    Alert GetAlert(const std::string& alertId) const;
    
    /*
     * @brief 获取所有活跃告警
     * @return 活跃告警列表
     */
    std::vector<Alert> GetActiveAlerts() const;
    
    /*
     * @brief 获取告警历史
     * @param startTime 开始时间
     * @param endTime 结束时间
     * @param limit 最大返回数量
     * @param offset 起始偏移
     * @return 告警历史列表
     */
    std::vector<Alert> GetAlertHistory(
        const std::chrono::system_clock::time_point& startTime,
        const std::chrono::system_clock::time_point& endTime,
        size_t limit = 100,
        size_t offset = 0) const;
    
    /*
     * @brief 设置告警回调
     * @param callback 回调函数
     */
    void SetAlertCallback(AlertCallback callback);
    
    /*
     * @brief 启动告警管理器
     * @return 是否成功启动
     */
    bool Start();
    
    /*
     * @brief 停止告警管理器
     */
    void Stop();
    
    /*
     * @brief 获取当前规则数量
     * @return 规则数量
     */
    size_t GetRuleCount() const;
    
    /*
     * @brief 获取当前通知渠道数量
     * @return 通知渠道数量
     */
    size_t GetChannelCount() const;
    
    /*
     * @brief 获取当前待处理告警数量
     * @return 待处理告警数量
     */
    size_t GetPendingAlertCount() const;
    
private:
    // 告警处理线程函数
    void AlertThreadFunc();
    
    // 检查线程函数
    void CheckThreadFunc();
    
    // 发送告警通知
    bool SendAlertNotification(const Alert& alert);
    
    // 保存告警到存储
    void SaveAlert(const Alert& alert);
    
    // 更新告警状态
    void UpdateAlertStatus(const std::string& alertId, AlertStatus status);
    
    // 检查是否是重复告警
    bool IsDuplicateAlert(const Alert& alert) const;
    
    // 生成唯一告警ID
    std::string GenerateAlertId() const;
    
    // 配置
    AlertManagerConfig config_;
    
    // 规则
    std::vector<std::shared_ptr<AlertRule>> rules_;
    mutable std::mutex rulesMutex_;
    
    // 通知渠道
    std::vector<std::shared_ptr<NotificationChannel>> channels_;
    mutable std::mutex channelsMutex_;
    
    // 告警队列
    std::queue<Alert> pendingAlerts_;
    mutable std::mutex alertsMutex_;
    size_t alertCount_{0};
    
    // 活跃告警
    std::unordered_map<std::string, Alert> activeAlerts_;
    mutable std::mutex activeAlertsMutex_;
    
    // 存储
    std::shared_ptr<storage::RedisStorage> redisStorage_;
    std::shared_ptr<storage::MySQLStorage> mysqlStorage_;
    
    // 告警线程
    std::thread alertThread_;
    std::thread checkThread_;
    std::atomic<bool> running_{false};
    
    // 回调函数
    AlertCallback alertCallback_;
    std::mutex callbackMutex_;
};

// 辅助函数：将告警级别转换为字符串
std::string AlertLevelToString(AlertLevel level);

// 辅助函数：将字符串转换为告警级别
AlertLevel AlertLevelFromString(const std::string& levelStr);

// 辅助函数：将告警状态转换为字符串
std::string AlertStatusToString(AlertStatus status);

// 辅助函数：将字符串转换为告警状态
AlertStatus AlertStatusFromString(const std::string& statusStr);

} // namespace alert
} // namespace xumj

#endif // XUMJ_ALERT_ALERT_MANAGER_H 