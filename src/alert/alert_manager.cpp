#include "xumj/alert/alert_manager.h"
#include "xumj/storage/storage_factory.h"
#include <iostream>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <chrono>
#include <uuid/uuid.h>
#include <nlohmann/json.hpp>
#include <curl/curl.h>

namespace xumj {
namespace alert {

// 辅助函数：获取当前时间字符串
std::string GetCurrentTimeStr() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = *std::localtime(&time_t_now);
    char buffer[30];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_now);
    return std::string(buffer);
}

// 辅助函数：时间点转字符串
std::string TimePointToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t = std::chrono::system_clock::to_time_t(tp);
    std::tm tm = *std::localtime(&time_t);
    char buffer[30];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buffer);
}

// 辅助函数：生成UUID
std::string GenerateUUID() {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    return std::string(uuid_str);
}

// 辅助函数：将告警级别转换为字符串
std::string AlertLevelToString(AlertLevel level) {
    switch (level) {
        case AlertLevel::INFO: return "INFO";
        case AlertLevel::WARNING: return "WARNING";
        case AlertLevel::ERROR: return "ERROR";
        case AlertLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// 辅助函数：将字符串转换为告警级别
AlertLevel AlertLevelFromString(const std::string& levelStr) {
    if (levelStr == "INFO") return AlertLevel::INFO;
    if (levelStr == "WARNING") return AlertLevel::WARNING;
    if (levelStr == "ERROR") return AlertLevel::ERROR;
    if (levelStr == "CRITICAL") return AlertLevel::CRITICAL;
    return AlertLevel::INFO;  // 默认为INFO
}

// 辅助函数：将告警状态转换为字符串
std::string AlertStatusToString(AlertStatus status) {
    switch (status) {
        case AlertStatus::PENDING: return "PENDING";
        case AlertStatus::ACTIVE: return "ACTIVE";
        case AlertStatus::RESOLVED: return "RESOLVED";
        case AlertStatus::IGNORED: return "IGNORED";
        default: return "UNKNOWN";
    }
}

// 辅助函数：将字符串转换为告警状态
AlertStatus AlertStatusFromString(const std::string& statusStr) {
    if (statusStr == "PENDING") return AlertStatus::PENDING;
    if (statusStr == "ACTIVE") return AlertStatus::ACTIVE;
    if (statusStr == "RESOLVED") return AlertStatus::RESOLVED;
    if (statusStr == "IGNORED") return AlertStatus::IGNORED;
    return AlertStatus::PENDING;  // 默认为PENDING
}

// 辅助函数：将告警转换为JSON字符串
std::string AlertToJson(const Alert& alert) {
    using json = nlohmann::json;
    
    json j;
    j["id"] = alert.id;
    j["name"] = alert.name;
    j["description"] = alert.description;
    j["level"] = AlertLevelToString(alert.level);
    j["status"] = AlertStatusToString(alert.status);
    j["source"] = alert.source;
    j["timestamp"] = TimePointToString(alert.timestamp);
    j["updateTime"] = TimePointToString(alert.updateTime);
    j["count"] = alert.count;
    
    // 添加标签
    json labels = json::object();
    for (const auto& [key, value] : alert.labels) {
        labels[key] = value;
    }
    j["labels"] = labels;
    
    // 添加注解
    json annotations = json::object();
    for (const auto& [key, value] : alert.annotations) {
        annotations[key] = value;
    }
    j["annotations"] = annotations;
    
    // 添加相关日志ID
    j["relatedLogIds"] = alert.relatedLogIds;
    
    return j.dump();
}

// CURL回调函数，用于接收响应数据
size_t CurlWriteCallback(char* ptr, size_t size, size_t nmemb, std::string* data) {
    data->append(ptr, size * nmemb);
    return size * nmemb;
}

// ThresholdAlertRule实现

ThresholdAlertRule::ThresholdAlertRule(
    const std::string& name,
    const std::string& description,
    const std::string& field,
    double threshold,
    const std::string& compareType,
    AlertLevel level)
    : name_(name), description_(description), field_(field),
      threshold_(threshold), compareType_(compareType), level_(level) {
}

bool ThresholdAlertRule::Check(
    const analyzer::LogRecord& /* record */,
    const std::unordered_map<std::string, std::string>& results) const {
    
    // 查找字段值
    auto it = results.find(field_);
    if (it == results.end()) {
        // 如果是record的基本字段，也进行检查
        if (field_ == "level") {
            it = results.find("record.level");
        } else if (field_ == "message") {
            it = results.find("record.message");
        } else if (field_ == "source") {
            it = results.find("record.source");
        } else {
            // 字段不存在
            return false;
        }
        
        // 再次检查是否找到
        if (it == results.end()) {
            return false;
        }
    }
    
    // 将字段值转换为数值进行比较
    double value;
    try {
        value = std::stod(it->second);
    } catch (const std::exception& e) {
        // 不是有效的数值
        return false;
    }
    
    // 根据比较类型进行比较
    if (compareType_ == ">") {
        return value > threshold_;
    } else if (compareType_ == "<") {
        return value < threshold_;
    } else if (compareType_ == ">=") {
        return value >= threshold_;
    } else if (compareType_ == "<=") {
        return value <= threshold_;
    } else if (compareType_ == "==") {
        return value == threshold_;
    } else if (compareType_ == "!=") {
        return value != threshold_;
    }
    
    return false;
}

Alert ThresholdAlertRule::GenerateAlert(
    const analyzer::LogRecord& record,
    const std::unordered_map<std::string, std::string>& results) const {
    
    Alert alert;
    alert.id = GenerateUUID();  // 实际使用时会被替换
    alert.name = name_;
    alert.description = description_;
    alert.level = level_;
    alert.status = AlertStatus::PENDING;
    alert.source = record.source;
    alert.timestamp = std::chrono::system_clock::now();
    alert.updateTime = alert.timestamp;
    
    // 添加标签
    alert.labels["rule"] = name_;
    alert.labels["field"] = field_;
    alert.labels["threshold"] = std::to_string(threshold_);
    alert.labels["compare_type"] = compareType_;
    
    // 添加注解
    std::ostringstream oss;
    auto fieldIt = results.find(field_);
    if (fieldIt != results.end()) {
        oss << "字段 " << field_ << " 的值 " << fieldIt->second 
            << " " << compareType_ << " " << threshold_;
    } else {
        oss << "触发阈值告警: " << compareType_ << " " << threshold_;
    }
    alert.annotations["summary"] = oss.str();
    alert.annotations["description"] = description_;
    
    // 添加相关日志ID
    alert.relatedLogIds.push_back(record.id);
    
    return alert;
}

std::string ThresholdAlertRule::GetName() const {
    return name_;
}

std::string ThresholdAlertRule::GetDescription() const {
    return description_;
}

// KeywordAlertRule实现

KeywordAlertRule::KeywordAlertRule(
    const std::string& name,
    const std::string& description,
    const std::string& field,
    const std::vector<std::string>& keywords,
    bool matchAll,
    AlertLevel level)
    : name_(name), description_(description), field_(field),
      keywords_(keywords), matchAll_(matchAll), level_(level) {
}

bool KeywordAlertRule::Check(
    const analyzer::LogRecord& record,
    const std::unordered_map<std::string, std::string>& results) const {
    
    // 查找字段值
    std::string content;
    
    if (field_ == "message") {
        content = record.message;
    } else if (field_ == "level") {
        content = record.level;
    } else if (field_ == "source") {
        content = record.source;
    } else {
        // 查找分析结果中的字段
        auto it = results.find(field_);
        if (it == results.end()) {
            // 字段不存在
            return false;
        }
        content = it->second;
    }
    
    // 将内容转为小写以进行不区分大小写的匹配
    std::string lowerContent = content;
    std::transform(lowerContent.begin(), lowerContent.end(), lowerContent.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    
    // 匹配关键字
    size_t matchCount = 0;
    
    for (const auto& keyword : keywords_) {
        // 关键字转小写
        std::string lowerKeyword = keyword;
        std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        // 查找关键字
        if (lowerContent.find(lowerKeyword) != std::string::npos) {
            matchCount++;
            
            // 如果不需要匹配所有关键字，匹配到一个就可以返回
            if (!matchAll_) {
                return true;
            }
        }
    }
    
    // 如果需要匹配所有关键字，检查是否所有关键字都匹配上了
    if (matchAll_) {
        return matchCount == keywords_.size();
    }
    
    return false;
}

Alert KeywordAlertRule::GenerateAlert(
    const analyzer::LogRecord& record,
    const std::unordered_map<std::string, std::string>& /* results */) const {
    
    Alert alert;
    alert.id = GenerateUUID();  // 实际使用时会被替换
    alert.name = name_;
    alert.description = description_;
    alert.level = level_;
    alert.status = AlertStatus::PENDING;
    alert.source = record.source;
    alert.timestamp = std::chrono::system_clock::now();
    alert.updateTime = alert.timestamp;
    
    // 添加标签
    alert.labels["rule"] = name_;
    alert.labels["field"] = field_;
    alert.labels["match_all"] = matchAll_ ? "true" : "false";
    
    // 添加注解
    std::ostringstream ossKeywords;
    for (size_t i = 0; i < keywords_.size(); ++i) {
        if (i > 0) ossKeywords << ", ";
        ossKeywords << keywords_[i];
    }
    
    alert.annotations["keywords"] = ossKeywords.str();
    alert.annotations["summary"] = "发现关键字: " + ossKeywords.str();
    alert.annotations["description"] = description_;
    
    // 添加相关日志ID
    alert.relatedLogIds.push_back(record.id);
    
    return alert;
}

std::string KeywordAlertRule::GetName() const {
    return name_;
}

std::string KeywordAlertRule::GetDescription() const {
    return description_;
}

// EmailNotificationChannel实现

EmailNotificationChannel::EmailNotificationChannel(
    const std::string& name,
    const std::string& smtpServer,
    int smtpPort,
    const std::string& username,
    const std::string& password,
    const std::string& from,
    const std::vector<std::string>& to,
    bool useTLS)
    : name_(name), smtpServer_(smtpServer), smtpPort_(smtpPort),
      username_(username), password_(password), from_(from), to_(to),
      useTLS_(useTLS) {
}

bool EmailNotificationChannel::SendAlert(const Alert& alert) {
    // 这里简化处理，实际实现中应该使用libcurl或其他库发送邮件
    std::cout << "发送邮件告警通知：" << std::endl;
    std::cout << "  SMTP服务器: " << smtpServer_ << ":" << smtpPort_ << std::endl;
    std::cout << "  发件人: " << from_ << std::endl;
    std::cout << "  收件人: ";
    for (const auto& to : to_) {
        std::cout << to << " ";
    }
    std::cout << std::endl;
    std::cout << "  主题: [" << AlertLevelToString(alert.level) << "] " << alert.name << std::endl;
    std::cout << "  内容: " << alert.description << std::endl;
    std::cout << "  时间: " << TimePointToString(alert.timestamp) << std::endl;
    
    // 在实际应用中，这里应该实现真正的邮件发送逻辑
    // 假设成功发送
    return true;
}

std::string EmailNotificationChannel::GetName() const {
    return name_;
}

std::string EmailNotificationChannel::GetType() const {
    return "EMAIL";
}

// WebhookNotificationChannel实现

WebhookNotificationChannel::WebhookNotificationChannel(
    const std::string& name,
    const std::string& url,
    const std::unordered_map<std::string, std::string>& headers,
    int timeout)
    : name_(name), url_(url), headers_(headers), timeout_(timeout) {
}

bool WebhookNotificationChannel::SendAlert(const Alert& alert) {
    // 使用libcurl发送HTTP请求
    CURL* curl = curl_easy_init();
    if (!curl) {
        std::cerr << "初始化CURL失败" << std::endl;
        return false;
    }
    
    // 将告警转换为JSON字符串
    std::string jsonData = AlertToJson(alert);
    
    // 设置URL
    curl_easy_setopt(curl, CURLOPT_URL, url_.c_str());
    
    // 设置HTTP POST请求
    curl_easy_setopt(curl, CURLOPT_POST, 1L);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonData.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, jsonData.length());
    
    // 设置HTTP头
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, "Accept: application/json");
    
    for (const auto& [key, value] : headers_) {
        std::string header = key + ": " + value;
        headers = curl_slist_append(headers, header.c_str());
    }
    
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    
    // 设置超时
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, timeout_);
    
    // 设置接收响应的回调
    std::string responseData;
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, CurlWriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
    
    // 发送请求
    CURLcode res = curl_easy_perform(curl);
    
    // 检查结果
    bool success = false;
    if (res == CURLE_OK) {
        long httpCode = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
        
        // 2xx表示成功
        if (httpCode >= 200 && httpCode < 300) {
            success = true;
        } else {
            std::cerr << "HTTP请求失败: " << httpCode << std::endl;
            std::cerr << "响应: " << responseData << std::endl;
        }
    } else {
        std::cerr << "CURL请求失败: " << curl_easy_strerror(res) << std::endl;
    }
    
    // 清理
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);
    
    return success;
}

std::string WebhookNotificationChannel::GetName() const {
    return name_;
}

std::string WebhookNotificationChannel::GetType() const {
    return "WEBHOOK";
}

// AlertManager实现

AlertManager::AlertManager(const AlertManagerConfig& config)
    : alertCount_(0), running_(false) {
    Initialize(config);
}

AlertManager::~AlertManager() {
    Stop();
}

bool AlertManager::Initialize(const AlertManagerConfig& config) {
    // 如果管理器已在运行，先停止它
    if (running_) {
        Stop();
    }
    
    // 保存配置
    config_ = config;
    
    // 初始化存储
    try {
        // 创建Redis存储
        if (!config_.redisConfigJson.empty()) {
            auto redisConfig = storage::StorageFactory::CreateRedisConfigFromJson(config_.redisConfigJson);
            redisStorage_ = storage::StorageFactory::CreateRedisStorage(redisConfig);
        }
        
        // 创建MySQL存储
        if (!config_.mysqlConfigJson.empty()) {
            auto mysqlConfig = storage::StorageFactory::CreateMySQLConfigFromJson(config_.mysqlConfigJson);
            mysqlStorage_ = storage::StorageFactory::CreateMySQLStorage(mysqlConfig);
            
            // 初始化MySQL表结构
            if (mysqlStorage_) {
                mysqlStorage_->Initialize();
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "初始化存储失败: " << e.what() << std::endl;
        return false;
    }
    
    // 初始化CURL库
    curl_global_init(CURL_GLOBAL_ALL);
    
    return true;
}

void AlertManager::AddRule(std::shared_ptr<AlertRule> rule) {
    if (rule) {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        rules_.push_back(std::move(rule));
    }
}

bool AlertManager::RemoveRule(const std::string& ruleName) {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    
    auto it = std::find_if(rules_.begin(), rules_.end(),
                         [&ruleName](const auto& rule) {
                             return rule->GetName() == ruleName;
                         });
    
    if (it != rules_.end()) {
        rules_.erase(it);
        return true;
    }
    
    return false;
}

void AlertManager::ClearRules() {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    rules_.clear();
}

void AlertManager::AddChannel(std::shared_ptr<NotificationChannel> channel) {
    if (channel) {
        std::lock_guard<std::mutex> lock(channelsMutex_);
        channels_.push_back(std::move(channel));
    }
}

bool AlertManager::RemoveChannel(const std::string& channelName) {
    std::lock_guard<std::mutex> lock(channelsMutex_);
    
    auto it = std::find_if(channels_.begin(), channels_.end(),
                         [&channelName](const auto& channel) {
                             return channel->GetName() == channelName;
                         });
    
    if (it != channels_.end()) {
        channels_.erase(it);
        return true;
    }
    
    return false;
}

void AlertManager::ClearChannels() {
    std::lock_guard<std::mutex> lock(channelsMutex_);
    channels_.clear();
}

std::vector<std::string> AlertManager::CheckAlerts(
    const analyzer::LogRecord& record,
    const std::unordered_map<std::string, std::string>& results) {
    
    std::vector<std::shared_ptr<AlertRule>> currentRules;
    
    // 获取当前规则（避免长时间持有锁）
    {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        currentRules = rules_;
    }
    
    std::vector<std::string> triggeredAlertIds;
    
    // 应用所有规则
    for (const auto& rule : currentRules) {
        // 检查是否触发规则
        if (rule->Check(record, results)) {
            // 生成告警
            Alert alert = rule->GenerateAlert(record, results);
            
            // 检查是否是重复告警
            if (config_.suppressDuplicates && IsDuplicateAlert(alert)) {
                // 更新现有告警
                std::string existingAlertId;
                
                {
                    std::lock_guard<std::mutex> lock(activeAlertsMutex_);
                    
                    // 查找具有相同名称和标签的告警
                    for (auto& [id, existingAlert] : activeAlerts_) {
                        if (existingAlert.name == alert.name &&
                            existingAlert.labels == alert.labels) {
                            
                            // 更新告警
                            existingAlert.count++;
                            existingAlert.updateTime = std::chrono::system_clock::now();
                            existingAlert.relatedLogIds.push_back(record.id);
                            
                            // 保存更新后的告警
                            SaveAlert(existingAlert);
                            
                            existingAlertId = id;
                            break;
                        }
                    }
                }
                
                if (!existingAlertId.empty()) {
                    triggeredAlertIds.push_back(existingAlertId);
                    continue;  // 跳过下面的新告警触发
                }
            }
            
            // 触发新告警
            std::string alertId = TriggerAlert(alert);
            triggeredAlertIds.push_back(alertId);
        }
    }
    
    return triggeredAlertIds;
}

std::string AlertManager::TriggerAlert(const Alert& alert) {
    // 创建一个告警的副本，设置ID和状态
    Alert newAlert = alert;
    newAlert.id = GenerateAlertId();
    newAlert.status = AlertStatus::ACTIVE;
    newAlert.timestamp = std::chrono::system_clock::now();
    newAlert.updateTime = newAlert.timestamp;
    
    // 保存告警
    SaveAlert(newAlert);
    
    // 添加到活跃告警列表
    {
        std::lock_guard<std::mutex> lock(activeAlertsMutex_);
        activeAlerts_[newAlert.id] = newAlert;
    }
    
    // 添加到待处理队列
    {
        std::lock_guard<std::mutex> lock(alertsMutex_);
        pendingAlerts_.push(newAlert);
        alertCount_++;
    }
    
    // 调用回调函数
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (alertCallback_) {
            alertCallback_(newAlert.id, newAlert.status);
        }
    }
    
    return newAlert.id;
}

bool AlertManager::ResolveAlert(const std::string& alertId, const std::string& comment) {
    // 查找并更新告警状态
    Alert alert;
    bool found = false;
    
    {
        std::lock_guard<std::mutex> lock(activeAlertsMutex_);
        
        auto it = activeAlerts_.find(alertId);
        if (it != activeAlerts_.end()) {
            alert = it->second;
            alert.status = AlertStatus::RESOLVED;
            alert.updateTime = std::chrono::system_clock::now();
            
            // 添加注释
            if (!comment.empty()) {
                alert.annotations["resolution_comment"] = comment;
            }
            
            // 更新活跃告警列表
            activeAlerts_.erase(it);
            
            found = true;
        }
    }
    
    if (found) {
        // 保存更新后的告警
        SaveAlert(alert);
        
        // 调用回调函数
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (alertCallback_) {
                alertCallback_(alertId, alert.status);
            }
        }
        
        return true;
    }
    
    return false;
}

bool AlertManager::IgnoreAlert(const std::string& alertId, const std::string& comment) {
    // 查找并更新告警状态
    Alert alert;
    bool found = false;
    
    {
        std::lock_guard<std::mutex> lock(activeAlertsMutex_);
        
        auto it = activeAlerts_.find(alertId);
        if (it != activeAlerts_.end()) {
            alert = it->second;
            alert.status = AlertStatus::IGNORED;
            alert.updateTime = std::chrono::system_clock::now();
            
            // 添加注释
            if (!comment.empty()) {
                alert.annotations["ignore_comment"] = comment;
            }
            
            // 更新活跃告警列表
            activeAlerts_.erase(it);
            
            found = true;
        }
    }
    
    if (found) {
        // 保存更新后的告警
        SaveAlert(alert);
        
        // 调用回调函数
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (alertCallback_) {
                alertCallback_(alertId, alert.status);
            }
        }
        
        return true;
    }
    
    return false;
}

Alert AlertManager::GetAlert(const std::string& alertId) const {
    // 先在活跃告警中查找
    {
        std::lock_guard<std::mutex> lock(activeAlertsMutex_);
        
        auto it = activeAlerts_.find(alertId);
        if (it != activeAlerts_.end()) {
            return it->second;
        }
    }
    
    // 从存储中查询
    if (redisStorage_) {
        try {
            std::string key = "alert:" + alertId;
            
            if (redisStorage_->Exists(key)) {
                std::string jsonData = redisStorage_->Get(key);
                
                // 解析JSON数据
                using json = nlohmann::json;
                json j = json::parse(jsonData);
                
                Alert alert;
                alert.id = j["id"];
                alert.name = j["name"];
                alert.description = j["description"];
                alert.level = AlertLevelFromString(j["level"]);
                alert.status = AlertStatusFromString(j["status"]);
                alert.source = j["source"];
                alert.count = j["count"];
                
                // 解析标签
                for (const auto& [key, value] : j["labels"].items()) {
                    alert.labels[key] = value;
                }
                
                // 解析注解
                for (const auto& [key, value] : j["annotations"].items()) {
                    alert.annotations[key] = value;
                }
                
                // 解析相关日志ID
                for (const auto& logId : j["relatedLogIds"]) {
                    alert.relatedLogIds.push_back(logId);
                }
                
                return alert;
            }
        } catch (const std::exception& e) {
            std::cerr << "从Redis获取告警失败: " << e.what() << std::endl;
        }
    }
    
    // 如果没有找到，返回空告警
    return Alert();
}

std::vector<Alert> AlertManager::GetActiveAlerts() const {
    std::vector<Alert> alerts;
    
    {
        std::lock_guard<std::mutex> lock(activeAlertsMutex_);
        alerts.reserve(activeAlerts_.size());
        
        for (const auto& [id, alert] : activeAlerts_) {
            alerts.push_back(alert);
        }
    }
    
    return alerts;
}

std::vector<Alert> AlertManager::GetAlertHistory(
    const std::chrono::system_clock::time_point& startTime,
    const std::chrono::system_clock::time_point& endTime,
    size_t limit,
    size_t offset) const {
    
    std::vector<Alert> alerts;
    
    // 从MySQL存储中查询历史
    if (mysqlStorage_) {
        try {
            // 转换时间点为字符串
            std::string startTimeStr = TimePointToString(startTime);
            std::string endTimeStr = TimePointToString(endTime);
            
            // 构造查询条件
            std::unordered_map<std::string, std::string> conditions;
            conditions["timestamp >= "] = startTimeStr;
            conditions["timestamp <= "] = endTimeStr;
            
            // 查询日志条目
            auto entries = mysqlStorage_->QueryLogEntries(conditions, limit, offset);
            
            for (const auto& entry : entries) {
                // 解析日志条目中的告警数据
                if (entry.fields.count("alert_data")) {
                    std::string jsonData = entry.fields.at("alert_data");
                    
                    try {
                        // 解析JSON数据
                        using json = nlohmann::json;
                        json j = json::parse(jsonData);
                        
                        Alert alert;
                        alert.id = j["id"];
                        alert.name = j["name"];
                        alert.description = j["description"];
                        alert.level = AlertLevelFromString(j["level"]);
                        alert.status = AlertStatusFromString(j["status"]);
                        alert.source = j["source"];
                        alert.count = j["count"];
                        
                        // 解析标签
                        for (const auto& [key, value] : j["labels"].items()) {
                            alert.labels[key] = value;
                        }
                        
                        // 解析注解
                        for (const auto& [key, value] : j["annotations"].items()) {
                            alert.annotations[key] = value;
                        }
                        
                        // 解析相关日志ID
                        for (const auto& logId : j["relatedLogIds"]) {
                            alert.relatedLogIds.push_back(logId);
                        }
                        
                        alerts.push_back(alert);
                    } catch (const std::exception& e) {
                        std::cerr << "解析告警JSON数据失败: " << e.what() << std::endl;
                    }
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "从MySQL查询告警历史失败: " << e.what() << std::endl;
        }
    }
    
    return alerts;
}

void AlertManager::SetAlertCallback(AlertCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    alertCallback_ = std::move(callback);
}

bool AlertManager::Start() {
    if (running_) {
        return true;  // 已经在运行
    }
    
    // 启动线程
    running_ = true;
    alertThread_ = std::thread(&AlertManager::AlertThreadFunc, this);
    checkThread_ = std::thread(&AlertManager::CheckThreadFunc, this);
    
    return true;
}

void AlertManager::Stop() {
    if (!running_) {
        return;  // 已经停止
    }
    
    // 停止线程
    running_ = false;
    
    // 等待线程结束
    if (alertThread_.joinable()) {
        alertThread_.join();
    }
    
    if (checkThread_.joinable()) {
        checkThread_.join();
    }
    
    // 清空队列
    {
        std::lock_guard<std::mutex> lock(alertsMutex_);
        while (!pendingAlerts_.empty()) {
            pendingAlerts_.pop();
        }
        alertCount_ = 0;
    }
    
    // 清理CURL库
    curl_global_cleanup();
}

size_t AlertManager::GetRuleCount() const {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    return rules_.size();
}

size_t AlertManager::GetChannelCount() const {
    std::lock_guard<std::mutex> lock(channelsMutex_);
    return channels_.size();
}

size_t AlertManager::GetPendingAlertCount() const {
    std::lock_guard<std::mutex> lock(alertsMutex_);
    return alertCount_;
}

void AlertManager::AlertThreadFunc() {
    while (running_) {
        std::vector<Alert> batch;
        
        // 获取一批待处理告警
        {
            std::lock_guard<std::mutex> lock(alertsMutex_);
            
            size_t count = std::min(config_.batchSize, alertCount_);
            for (size_t i = 0; i < count && !pendingAlerts_.empty(); ++i) {
                batch.push_back(pendingAlerts_.front());
                pendingAlerts_.pop();
            }
            alertCount_ -= batch.size();
        }
        
        // 处理当前批次
        for (const auto& alert : batch) {
            // 发送告警通知
            SendAlertNotification(alert);
        }
        
        // 等待指定的间隔
        if (batch.empty()) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
    }
}

void AlertManager::CheckThreadFunc() {
    while (running_) {
        // 定期检查活跃告警，看是否需要重新发送
        {
            std::lock_guard<std::mutex> lock(activeAlertsMutex_);
            
            auto now = std::chrono::system_clock::now();
            
            for (auto& [id, alert] : activeAlerts_) {
                // 如果上次更新时间超过了重发间隔，重新添加到待处理队列
                if (now - alert.updateTime > config_.resendInterval) {
                    alert.updateTime = now;  // 更新时间
                    
                    // 添加到待处理队列
                    {
                        std::lock_guard<std::mutex> alertLock(alertsMutex_);
                        pendingAlerts_.push(alert);
                        alertCount_++;
                    }
                }
            }
        }
        
        // 等待检查间隔
        std::this_thread::sleep_for(config_.checkInterval);
    }
}

bool AlertManager::SendAlertNotification(const Alert& alert) {
    std::vector<std::shared_ptr<NotificationChannel>> currentChannels;
    
    // 获取当前通知渠道（避免长时间持有锁）
    {
        std::lock_guard<std::mutex> lock(channelsMutex_);
        currentChannels = channels_;
    }
    
    bool allSuccess = true;
    
    // 发送到所有通知渠道
    for (const auto& channel : currentChannels) {
        try {
            bool success = channel->SendAlert(alert);
            if (!success) {
                std::cerr << "发送告警通知失败: " << channel->GetName() 
                          << " (" << channel->GetType() << ")" << std::endl;
                allSuccess = false;
            }
        } catch (const std::exception& e) {
            std::cerr << "发送告警通知异常: " << e.what() << std::endl;
            allSuccess = false;
        }
    }
    
    return allSuccess;
}

void AlertManager::SaveAlert(const Alert& alert) {
    // 保存到Redis
    if (redisStorage_) {
        try {
            std::string key = "alert:" + alert.id;
            std::string jsonData = AlertToJson(alert);
            
            // 保存告警数据
            redisStorage_->Set(key, jsonData);
            
            // 设置过期时间（7天）
            redisStorage_->Expire(key, 86400 * 7);
            
            // 根据状态添加到相应的集合
            std::string statusKey = "alerts:" + AlertStatusToString(alert.status);
            redisStorage_->SetAdd(statusKey, alert.id);
            
            // 如果是活跃告警，添加到活跃告警集合
            if (alert.status == AlertStatus::ACTIVE) {
                redisStorage_->SetAdd("alerts:active", alert.id);
            } else {
                // 如果不是活跃告警，从活跃告警集合中移除
                redisStorage_->SetRemove("alerts:active", alert.id);
            }
        } catch (const std::exception& e) {
            std::cerr << "保存告警到Redis失败: " << e.what() << std::endl;
        }
    }
    
    // 保存到MySQL
    if (mysqlStorage_) {
        try {
            storage::MySQLStorage::LogEntry entry;
            entry.id = alert.id;
            entry.timestamp = TimePointToString(alert.timestamp);
            entry.level = AlertLevelToString(alert.level);
            entry.source = alert.source;
            entry.message = alert.name + ": " + alert.description;
            
            // 将告警数据保存为JSON字段
            entry.fields["alert_data"] = AlertToJson(alert);
            entry.fields["alert_status"] = AlertStatusToString(alert.status);
            entry.fields["alert_name"] = alert.name;
            
            // 保存到MySQL
            mysqlStorage_->SaveLogEntry(entry);
        } catch (const std::exception& e) {
            std::cerr << "保存告警到MySQL失败: " << e.what() << std::endl;
        }
    }
}

bool AlertManager::IsDuplicateAlert(const Alert& alert) const {
    std::lock_guard<std::mutex> lock(activeAlertsMutex_);
    
    // 检查是否有相同名称和标签的活跃告警
    for (const auto& [id, existingAlert] : activeAlerts_) {
        if (existingAlert.name == alert.name && existingAlert.labels == alert.labels) {
            return true;
        }
    }
    
    return false;
}

std::string AlertManager::GenerateAlertId() const {
    return "alert-" + GenerateUUID();
}

} // namespace alert
} // namespace xumj 