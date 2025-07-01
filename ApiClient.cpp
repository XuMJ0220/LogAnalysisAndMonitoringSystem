#include "ApiClient.h"
#include <QNetworkRequest>
#include <QNetworkReply>
#include <QJsonDocument>
#include <QSettings>
#include <QUrlQuery>
#include <QDebug>

namespace xumj {

ApiClient::ApiClient(QObject *parent) : QObject(parent) {
    manager = new QNetworkAccessManager(this);
    QSettings settings;
    apiBaseUrl = settings.value("api/baseUrl", "http://localhost:18080").toString();
}

void ApiClient::setApiBaseUrl(const QString &url) {
    apiBaseUrl = url;
    QSettings settings;
    settings.setValue("api/baseUrl", url);
}

QString ApiClient::getApiBaseUrl() const {
    return apiBaseUrl;
}

void ApiClient::handleNetworkReply(QNetworkReply *reply, std::function<void(const QJsonDocument &)> handler) {
    connect(reply, &QNetworkReply::finished, [this, reply, handler]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(response);
            handler(doc);
        } else {
            emit errorOccurred(reply->errorString());
        }
        reply->deleteLater();
    });
}

void ApiClient::getLogs(int limit, int offset, const QString &query) {
    QUrlQuery urlQuery;
    urlQuery.addQueryItem("limit", QString::number(limit));
    urlQuery.addQueryItem("offset", QString::number(offset));
    if (!query.isEmpty()) {
        urlQuery.addQueryItem("query", query);
    }
    QUrl url(apiBaseUrl + "/api/logs");
    url.setQuery(urlQuery);
    qDebug() << "Requesting logs from:" << url.toString();
    QNetworkRequest request(url);
    QNetworkReply *reply = manager->get(request);
    handleNetworkReply(reply, [this](const QJsonDocument &doc) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("success") && obj["success"].toBool() && 
                obj.contains("data") && obj["data"].isObject()) {
                QJsonObject data = obj["data"].toObject();
                QJsonArray logs = data.contains("logs") ? data["logs"].toArray() : QJsonArray();
                int totalCount = data.contains("total_count") ? data["total_count"].toInt() : 0;
                emit logsReceived(logs, totalCount);
            }
        }
    });
}

void ApiClient::getLogs(const QUrlQuery &query) {
    QUrl url(apiBaseUrl + "/api/logs");
    url.setQuery(query);
    qDebug() << "Requesting logs from:" << url.toString();
    QNetworkRequest request(url);
    QNetworkReply *reply = manager->get(request);
    handleNetworkReply(reply, [this](const QJsonDocument &doc) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("success") && obj["success"].toBool() && 
                obj.contains("data") && obj["data"].isObject()) {
                QJsonObject data = obj["data"].toObject();
                QJsonArray logs = data.contains("logs") ? data["logs"].toArray() : QJsonArray();
                int totalCount = data.contains("total_count") ? data["total_count"].toInt() : 0;
                emit logsReceived(logs, totalCount);
            }
        }
    });
}

void ApiClient::getLogStats() {
    QNetworkRequest request(QUrl(apiBaseUrl + "/api/logs/stats"));
    QNetworkReply *reply = manager->get(request);
    handleNetworkReply(reply, [this](const QJsonDocument &doc) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("success") && obj["success"].toBool() && 
                obj.contains("data") && obj["data"].isObject()) {
                emit logStatsReceived(obj["data"].toObject());
            }
        }
    });
}

void ApiClient::checkHealth() {
    QNetworkRequest request(QUrl(apiBaseUrl + "/health"));
    QNetworkReply *reply = manager->get(request);
    connect(reply, &QNetworkReply::finished, [this, reply]() {
        if (reply->error() == QNetworkReply::NoError) {
            QByteArray response = reply->readAll();
            QJsonDocument doc = QJsonDocument::fromJson(response);
            if (doc.isObject()) {
                QJsonObject obj = doc.object();
                bool success = obj.contains("success") && obj["success"].toBool();
                QString message = obj.contains("message") ? obj["message"].toString() : "";
                emit healthStatusReceived(success, message);
            }
        } else {
            emit healthStatusReceived(false, reply->errorString());
            emit errorOccurred(reply->errorString());
        }
        reply->deleteLater();
    });
}

// 告警相关接口实现

void ApiClient::getAlerts(const QUrlQuery &query) {
    QUrl url(apiBaseUrl + "/api/alerts");
    if (!query.isEmpty()) {
        url.setQuery(query);
    }
    QNetworkRequest request(url);
    QNetworkReply *reply = manager->get(request);
    handleNetworkReply(reply, [this](const QJsonDocument &doc) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("success") && obj["success"].toBool() && 
                obj.contains("data") && obj["data"].isObject()) {
                QJsonObject data = obj["data"].toObject();
                QJsonArray alerts = data.contains("alerts") ? data["alerts"].toArray() : QJsonArray();
                emit alertsReceived(alerts);
            }
        }
    });
}

void ApiClient::getAlertDetail(const QString &alertId) {
    QUrl url(apiBaseUrl + "/api/alerts/" + alertId);
    QNetworkRequest request(url);
    QNetworkReply *reply = manager->get(request);
    handleNetworkReply(reply, [this](const QJsonDocument &doc) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("success") && obj["success"].toBool() && 
                obj.contains("data") && obj["data"].isObject()) {
                emit alertDetailReceived(obj["data"].toObject());
            }
        }
    });
}

void ApiClient::updateAlertStatus(const QString &alertId, const QString &status, const QString &comment) {
    QUrl url(apiBaseUrl + "/api/alerts/" + alertId + "/status");
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    
    QJsonObject jsonObj;
    jsonObj["status"] = status;
    if (!comment.isEmpty()) {
        jsonObj["comment"] = comment;
    }
    
    QJsonDocument doc(jsonObj);
    QByteArray data = doc.toJson();
    
    QNetworkReply *reply = manager->put(request, data);
    handleNetworkReply(reply, [this, alertId](const QJsonDocument &doc) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            bool success = obj.contains("success") && obj["success"].toBool();
            emit alertStatusUpdated(alertId, success);
        }
    });
}

// 告警规则相关接口实现

void ApiClient::getAlertRules() {
    QNetworkRequest request(QUrl(apiBaseUrl + "/api/rules"));
    QNetworkReply *reply = manager->get(request);
    handleNetworkReply(reply, [this](const QJsonDocument &doc) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("success") && obj["success"].toBool() && 
                obj.contains("data") && obj["data"].isObject()) {
                QJsonObject data = obj["data"].toObject();
                QJsonArray rules = data.contains("rules") ? data["rules"].toArray() : QJsonArray();
                emit alertRulesReceived(rules);
            }
        }
    });
}

void ApiClient::createAlertRule(const QJsonObject &rule) {
    QNetworkRequest request(QUrl(apiBaseUrl + "/api/rules"));
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonDocument doc(rule);
    QByteArray data = doc.toJson();
    
    QNetworkReply *reply = manager->post(request, data);
    handleNetworkReply(reply, [this](const QJsonDocument &doc) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            bool success = obj.contains("success") && obj["success"].toBool();
            QString message = obj.contains("message") ? obj["message"].toString() : "";
            emit alertRuleCreated(success, message);
        }
    });
}

void ApiClient::updateAlertRule(const QString &ruleId, const QJsonObject &rule) {
    QUrl url(apiBaseUrl + "/api/rules/" + ruleId);
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
    QJsonDocument doc(rule);
    QByteArray data = doc.toJson();
    
    QNetworkReply *reply = manager->put(request, data);
    handleNetworkReply(reply, [this](const QJsonDocument &doc) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            bool success = obj.contains("success") && obj["success"].toBool();
            QString message = obj.contains("message") ? obj["message"].toString() : "";
            emit alertRuleUpdated(success, message);
        }
    });
}

void ApiClient::deleteAlertRule(const QString &ruleId) {
    QUrl url(apiBaseUrl + "/api/rules/" + ruleId);
    QNetworkRequest request(url);
    
    QNetworkReply *reply = manager->deleteResource(request);
    handleNetworkReply(reply, [this](const QJsonDocument &doc) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            bool success = obj.contains("success") && obj["success"].toBool();
            QString message = obj.contains("message") ? obj["message"].toString() : "";
            emit alertRuleDeleted(success, message);
        }
    });
}

// 系统监控相关接口实现

void ApiClient::getSystemStatus() {
    QNetworkRequest request(QUrl(apiBaseUrl + "/api/system/status"));
    QNetworkReply *reply = manager->get(request);
    handleNetworkReply(reply, [this](const QJsonDocument &doc) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("success") && obj["success"].toBool() && 
                obj.contains("data") && obj["data"].isObject()) {
                emit systemStatusReceived(obj["data"].toObject());
            }
        }
    });
}

void ApiClient::getMetrics(const QString &component) {
    QUrl url(apiBaseUrl + "/api/system/metrics");
    if (!component.isEmpty()) {
        QUrlQuery query;
        query.addQueryItem("component", component);
        url.setQuery(query);
    }
    QNetworkRequest request(url);
    QNetworkReply *reply = manager->get(request);
    handleNetworkReply(reply, [this](const QJsonDocument &doc) {
        if (doc.isObject()) {
            QJsonObject obj = doc.object();
            if (obj.contains("success") && obj["success"].toBool() && 
                obj.contains("data") && obj["data"].isObject()) {
                emit metricsReceived(obj["data"].toObject());
            }
        }
    });
}

} // namespace xumj 