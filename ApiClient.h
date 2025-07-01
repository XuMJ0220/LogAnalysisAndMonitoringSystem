#pragma once
#include <QObject>
#include <QNetworkAccessManager>
#include <QJsonArray>
#include <QJsonObject>
#include <QSettings>
#include <QUrlQuery>

namespace xumj {

class ApiClient : public QObject {
    Q_OBJECT
public:
    explicit ApiClient(QObject *parent = nullptr);
    void setApiBaseUrl(const QString &url);
    QString getApiBaseUrl() const;
    void getLogs(int limit = 100, int offset = 0, const QString &query = QString());
    void getLogs(const QUrlQuery &query);
    void getLogStats();
    void checkHealth();
    
    // 告警相关接口
    void getAlerts(const QUrlQuery &query = QUrlQuery());
    void getAlertDetail(const QString &alertId);
    void updateAlertStatus(const QString &alertId, const QString &status, const QString &comment = QString());
    
    // 告警规则相关接口
    void getAlertRules();
    void createAlertRule(const QJsonObject &rule);
    void updateAlertRule(const QString &ruleId, const QJsonObject &rule);
    void deleteAlertRule(const QString &ruleId);
    
    // 系统监控相关接口
    void getSystemStatus();
    void getMetrics(const QString &component = QString());

signals:
    void logsReceived(const QJsonArray &logs, int totalCount);
    void logStatsReceived(const QJsonObject &stats);
    void healthStatusReceived(bool healthy, const QString &message);
    void errorOccurred(const QString &errorMessage);
    
    // 告警相关信号
    void alertsReceived(const QJsonArray &alerts);
    void alertDetailReceived(const QJsonObject &alert);
    void alertStatusUpdated(const QString &alertId, bool success);
    
    // 告警规则相关信号
    void alertRulesReceived(const QJsonArray &rules);
    void alertRuleCreated(bool success, const QString &message);
    void alertRuleUpdated(bool success, const QString &message);
    void alertRuleDeleted(bool success, const QString &message);
    
    // 系统监控相关信号
    void systemStatusReceived(const QJsonObject &status);
    void metricsReceived(const QJsonObject &metrics);

private:
    QNetworkAccessManager *manager;
    QString apiBaseUrl = "http://localhost:18080";
    void handleNetworkReply(QNetworkReply *reply, std::function<void(const QJsonDocument &)> handler);
};

} // namespace xumj 