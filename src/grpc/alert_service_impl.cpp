#include "alert_service_impl.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <thread>

namespace xumj {
namespace grpc {

// 构造函数
AlertServiceImpl::AlertServiceImpl(std::shared_ptr<alert::AlertManager> alertManager)
    : alertManager_(alertManager) {
    
    if (!alertManager_) {
        throw std::runtime_error("告警管理器不能为空");
    }
    
    // 设置告警回调，用于通知订阅者
    alertManager_->SetAlertCallback([this](const std::string& alertId, alert::AlertStatus status) {
        // 获取告警对象
        auto alert = alertManager_->GetAlert(alertId);
        if (alert) {
            // 转换为Proto并通知订阅者
            auto protoAlert = ConvertToProto(*alert);
            NotifySubscribers(protoAlert);
        }
    });
}

// 获取告警列表
::grpc::Status AlertServiceImpl::GetAlerts(
    ::grpc::ServerContext* context,
    const ::xumj::proto::AlertQuery* request,
    ::xumj::proto::AlertQueryResponse* response) {
    
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "请求为空");
    }
    
    // 获取告警列表
    auto alerts = alertManager_->GetAlerts();
    
    // 根据查询条件过滤
    std::vector<alert::Alert> filteredAlerts;
    for (const auto& alert : alerts) {
        bool matched = true;
        
        // 检查开始时间
        if (!request->start_time().empty()) {
            if (alert.timestamp < std::chrono::system_clock::from_time_t(std::stoll(request->start_time()))) {
                matched = false;
            }
        }
        
        // 检查结束时间
        if (!request->end_time().empty()) {
            if (alert.timestamp > std::chrono::system_clock::from_time_t(std::stoll(request->end_time()))) {
                matched = false;
            }
        }
        
        // 检查级别
        if (request->levels_size() > 0) {
            bool levelMatched = false;
            for (int i = 0; i < request->levels_size(); ++i) {
                if (static_cast<int>(alert.level) == request->levels(i)) {
                    levelMatched = true;
                    break;
                }
            }
            
            if (!levelMatched) {
                matched = false;
            }
        }
        
        // 检查状态
        if (request->statuses_size() > 0) {
            bool statusMatched = false;
            for (int i = 0; i < request->statuses_size(); ++i) {
                if (static_cast<int>(alert.status) == request->statuses(i)) {
                    statusMatched = true;
                    break;
                }
            }
            
            if (!statusMatched) {
                matched = false;
            }
        }
        
        // 检查来源
        if (request->sources_size() > 0) {
            bool sourceMatched = false;
            for (int i = 0; i < request->sources_size(); ++i) {
                if (alert.source == request->sources(i)) {
                    sourceMatched = true;
                    break;
                }
            }
            
            if (!sourceMatched) {
                matched = false;
            }
        }
        
        // 检查名称模式
        if (!request->name_pattern().empty()) {
            if (alert.name.find(request->name_pattern()) == std::string::npos) {
                matched = false;
            }
        }
        
        // 检查标签过滤器
        for (const auto& [key, value] : request->label_filters()) {
            auto it = alert.labels.find(key);
            if (it == alert.labels.end() || it->second != value) {
                matched = false;
                break;
            }
        }
        
        if (matched) {
            filteredAlerts.push_back(alert);
        }
    }
    
    // 应用分页
    int totalCount = filteredAlerts.size();
    int offset = request->offset();
    int limit = request->limit() > 0 ? request->limit() : 100;
    
    // 验证偏移量
    if (offset >= totalCount) {
        offset = 0;
    }
    
    // 计算结束位置
    int endPos = std::min(offset + limit, totalCount);
    
    // 设置响应
    response->set_success(true);
    response->set_message("查询成功");
    response->set_total_count(totalCount);
    response->set_has_more(endPos < totalCount);
    
    // 添加告警
    for (int i = offset; i < endPos; ++i) {
        *response->add_alerts() = ConvertToProto(filteredAlerts[i]);
    }
    
    return ::grpc::Status::OK;
}

// 获取告警详情
::grpc::Status AlertServiceImpl::GetAlertDetail(
    ::grpc::ServerContext* context,
    const ::xumj::proto::AlertDetailRequest* request,
    ::xumj::proto::Alert* response) {
    
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "请求为空");
    }
    
    // 获取告警
    auto alert = alertManager_->GetAlert(request->alert_id());
    if (!alert) {
        return ::grpc::Status(::grpc::StatusCode::NOT_FOUND, "未找到告警");
    }
    
    // 设置响应
    *response = ConvertToProto(*alert);
    
    return ::grpc::Status::OK;
}

// 更新告警状态
::grpc::Status AlertServiceImpl::UpdateAlertStatus(
    ::grpc::ServerContext* context,
    const ::xumj::proto::UpdateAlertStatusRequest* request,
    ::xumj::proto::UpdateAlertStatusResponse* response) {
    
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "请求为空");
    }
    
    bool success = false;
    std::string alertId = request->alert_id();
    alert::AlertStatus status = static_cast<alert::AlertStatus>(request->status());
    std::string comment = request->comment();
    
    // 根据状态执行不同操作
    switch (status) {
        case alert::AlertStatus::RESOLVED:
            success = alertManager_->ResolveAlert(alertId, comment);
            break;
            
        case alert::AlertStatus::IGNORED:
            success = alertManager_->IgnoreAlert(alertId, comment);
            break;
            
        default:
            return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "不支持的状态");
    }
    
    // 设置响应
    response->set_success(success);
    if (success) {
        response->set_message("更新状态成功");
        auto alert = alertManager_->GetAlert(alertId);
        if (alert) {
            *response->mutable_updated_alert() = ConvertToProto(*alert);
        }
    } else {
        response->set_message("更新状态失败");
    }
    
    return ::grpc::Status::OK;
}

// 创建告警规则
::grpc::Status AlertServiceImpl::CreateAlertRule(
    ::grpc::ServerContext* context,
    const ::xumj::proto::AlertRule* request,
    ::xumj::proto::CreateAlertRuleResponse* response) {
    
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "请求为空");
    }
    
    // 转换为内部规则对象
    auto rule = ConvertRuleFromProto(*request);
    if (!rule) {
        response->set_success(false);
        response->set_message("转换规则失败");
        return ::grpc::Status::OK;
    }
    
    // 添加规则
    std::string ruleId = alertManager_->AddRule(rule);
    
    // 设置响应
    response->set_success(!ruleId.empty());
    if (!ruleId.empty()) {
        response->set_message("创建规则成功");
        response->set_rule_id(ruleId);
    } else {
        response->set_message("创建规则失败");
    }
    
    return ::grpc::Status::OK;
}

// 更新告警规则
::grpc::Status AlertServiceImpl::UpdateAlertRule(
    ::grpc::ServerContext* context,
    const ::xumj::proto::AlertRule* request,
    ::xumj::proto::UpdateAlertRuleResponse* response) {
    
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "请求为空");
    }
    
    // 检查规则ID
    if (request->id().empty()) {
        response->set_success(false);
        response->set_message("规则ID不能为空");
        return ::grpc::Status::OK;
    }
    
    // 检查规则是否存在
    auto existingRule = alertManager_->GetRule(request->id());
    if (!existingRule) {
        response->set_success(false);
        response->set_message("规则不存在");
        return ::grpc::Status::OK;
    }
    
    // 转换为内部规则对象
    auto rule = ConvertRuleFromProto(*request);
    if (!rule) {
        response->set_success(false);
        response->set_message("转换规则失败");
        return ::grpc::Status::OK;
    }
    
    // 更新规则
    bool success = alertManager_->UpdateRule(request->id(), rule);
    
    // 设置响应
    response->set_success(success);
    if (success) {
        response->set_message("更新规则成功");
    } else {
        response->set_message("更新规则失败");
    }
    
    return ::grpc::Status::OK;
}

// 删除告警规则
::grpc::Status AlertServiceImpl::DeleteAlertRule(
    ::grpc::ServerContext* context,
    const ::xumj::proto::DeleteAlertRuleRequest* request,
    ::xumj::proto::DeleteAlertRuleResponse* response) {
    
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "请求为空");
    }
    
    // 删除规则
    bool success = alertManager_->RemoveRule(request->rule_id());
    
    // 设置响应
    response->set_success(success);
    if (success) {
        response->set_message("删除规则成功");
    } else {
        response->set_message("删除规则失败");
    }
    
    return ::grpc::Status::OK;
}

// 获取告警规则列表
::grpc::Status AlertServiceImpl::GetAlertRules(
    ::grpc::ServerContext* context,
    const ::xumj::proto::AlertRuleQuery* request,
    ::xumj::proto::AlertRuleQueryResponse* response) {
    
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "请求为空");
    }
    
    // 获取规则列表
    auto rules = alertManager_->GetRules();
    
    // 根据查询条件过滤
    std::vector<std::shared_ptr<alert::AlertRule>> filteredRules;
    for (const auto& rule : rules) {
        bool matched = true;
        
        // 检查类型
        if (request->types_size() > 0) {
            bool typeMatched = false;
            // 这里需要实现规则类型判断逻辑
            // ...
            
            if (!typeMatched) {
                matched = false;
            }
        }
        
        // 检查级别
        if (request->levels_size() > 0) {
            bool levelMatched = false;
            for (int i = 0; i < request->levels_size(); ++i) {
                if (static_cast<int>(rule->GetLevel()) == request->levels(i)) {
                    levelMatched = true;
                    break;
                }
            }
            
            if (!levelMatched) {
                matched = false;
            }
        }
        
        // 检查名称模式
        if (!request->name_pattern().empty()) {
            if (rule->GetName().find(request->name_pattern()) == std::string::npos) {
                matched = false;
            }
        }
        
        // 检查标签过滤器
        // 这里需要实现规则标签过滤逻辑
        // ...
        
        if (matched) {
            filteredRules.push_back(rule);
        }
    }
    
    // 应用分页
    int totalCount = filteredRules.size();
    int offset = request->offset();
    int limit = request->limit() > 0 ? request->limit() : 100;
    
    // 验证偏移量
    if (offset >= totalCount) {
        offset = 0;
    }
    
    // 计算结束位置
    int endPos = std::min(offset + limit, totalCount);
    
    // 设置响应
    response->set_success(true);
    response->set_message("查询成功");
    response->set_total_count(totalCount);
    response->set_has_more(endPos < totalCount);
    
    // 添加规则
    for (int i = offset; i < endPos; ++i) {
        *response->add_rules() = ConvertRuleToProto(filteredRules[i]);
    }
    
    return ::grpc::Status::OK;
}

// 订阅告警
::grpc::Status AlertServiceImpl::SubscribeAlerts(
    ::grpc::ServerContext* context,
    const ::xumj::proto::SubscribeAlertsRequest* request,
    ::grpc::ServerWriter<::xumj::proto::Alert>* writer) {
    
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "请求为空");
    }
    
    // 添加订阅者
    std::string clientId = request->client_id();
    if (clientId.empty()) {
        clientId = "anonymous-" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
    }
    
    {
        std::lock_guard<std::mutex> lock(subscribersMutex_);
        
        subscribers_.push_back({
            clientId,
            [writer](const xumj::proto::Alert& alert) {
                writer->Write(alert);
            },
            *request
        });
    }
    
    std::cout << "客户端 " << clientId << " 已订阅告警" << std::endl;
    
    // 等待客户端断开连接或上下文取消
    context->AsyncNotifyWhenDone(nullptr);
    while (!context->IsCancelled()) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 移除订阅者
    {
        std::lock_guard<std::mutex> lock(subscribersMutex_);
        
        subscribers_.erase(
            std::remove_if(subscribers_.begin(), subscribers_.end(),
                          [&clientId](const Subscriber& subscriber) {
                              return subscriber.clientId == clientId;
                          }),
            subscribers_.end());
    }
    
    std::cout << "客户端 " << clientId << " 已取消订阅" << std::endl;
    
    return ::grpc::Status::OK;
}

// 通知所有订阅者
void AlertServiceImpl::NotifySubscribers(const xumj::proto::Alert& alert) {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    
    for (const auto& subscriber : subscribers_) {
        if (MatchesFilter(alert, subscriber.filter)) {
            try {
                subscriber.callback(alert);
            } catch (const std::exception& e) {
                std::cerr << "通知订阅者失败: " << e.what() << std::endl;
            }
        }
    }
}

// 转换Alert到Proto
xumj::proto::Alert AlertServiceImpl::ConvertToProto(const alert::Alert& alert) {
    xumj::proto::Alert proto;
    
    proto.set_id(alert.id);
    proto.set_name(alert.name);
    proto.set_description(alert.description);
    proto.set_level(static_cast<xumj::proto::AlertLevel>(alert.level));
    proto.set_status(static_cast<xumj::proto::AlertStatus>(alert.status));
    proto.set_source(alert.source);
    
    // 设置时间戳
    auto timestamp = std::chrono::system_clock::to_time_t(alert.timestamp);
    proto.set_timestamp(std::to_string(timestamp));
    
    // 设置更新时间
    auto updateTime = std::chrono::system_clock::to_time_t(alert.updateTime);
    proto.set_update_time(std::to_string(updateTime));
    
    // 添加标签
    for (const auto& [key, value] : alert.labels) {
        (*proto.mutable_labels())[key] = value;
    }
    
    // 添加注解
    for (const auto& [key, value] : alert.annotations) {
        (*proto.mutable_annotations())[key] = value;
    }
    
    // 添加关联日志ID
    for (const auto& logId : alert.relatedLogIds) {
        proto.add_related_log_ids(logId);
    }
    
    proto.set_count(alert.count);
    proto.set_resolution_comment(alert.resolutionComment);
    
    return proto;
}

// 转换Proto到Alert
alert::Alert AlertServiceImpl::ConvertFromProto(const xumj::proto::Alert& proto) {
    alert::Alert alert;
    
    alert.id = proto.id();
    alert.name = proto.name();
    alert.description = proto.description();
    alert.level = static_cast<alert::AlertLevel>(proto.level());
    alert.status = static_cast<alert::AlertStatus>(proto.status());
    alert.source = proto.source();
    
    // 设置时间戳
    if (!proto.timestamp().empty()) {
        try {
            auto timestamp = std::stoull(proto.timestamp());
            alert.timestamp = std::chrono::system_clock::from_time_t(timestamp);
        } catch (const std::exception& e) {
            std::cerr << "转换时间戳失败: " << e.what() << std::endl;
            alert.timestamp = std::chrono::system_clock::now();
        }
    } else {
        alert.timestamp = std::chrono::system_clock::now();
    }
    
    // 设置更新时间
    if (!proto.update_time().empty()) {
        try {
            auto updateTime = std::stoull(proto.update_time());
            alert.updateTime = std::chrono::system_clock::from_time_t(updateTime);
        } catch (const std::exception& e) {
            std::cerr << "转换更新时间失败: " << e.what() << std::endl;
            alert.updateTime = std::chrono::system_clock::now();
        }
    } else {
        alert.updateTime = std::chrono::system_clock::now();
    }
    
    // 添加标签
    for (const auto& [key, value] : proto.labels()) {
        alert.labels[key] = value;
    }
    
    // 添加注解
    for (const auto& [key, value] : proto.annotations()) {
        alert.annotations[key] = value;
    }
    
    // 添加关联日志ID
    for (int i = 0; i < proto.related_log_ids_size(); ++i) {
        alert.relatedLogIds.push_back(proto.related_log_ids(i));
    }
    
    alert.count = proto.count();
    alert.resolutionComment = proto.resolution_comment();
    
    return alert;
}

// 转换AlertRule到Proto
xumj::proto::AlertRule AlertServiceImpl::ConvertRuleToProto(const std::shared_ptr<alert::AlertRule>& rule) {
    xumj::proto::AlertRule proto;
    
    // 设置基本属性
    proto.set_id(rule->GetId());
    proto.set_name(rule->GetName());
    proto.set_description(rule->GetDescription());
    proto.set_level(static_cast<xumj::proto::AlertLevel>(rule->GetLevel()));
    proto.set_enabled(true);
    
    // 根据规则类型设置不同的配置
    if (auto thresholdRule = std::dynamic_pointer_cast<alert::ThresholdAlertRule>(rule)) {
        proto.set_type(xumj::proto::AlertRuleType::THRESHOLD);
        
        auto thresholdConfig = proto.mutable_threshold_config();
        thresholdConfig->set_field(thresholdRule->GetField());
        thresholdConfig->set_threshold(thresholdRule->GetThreshold());
        thresholdConfig->set_compare_type(thresholdRule->GetCompareOperator());
    } else if (auto keywordRule = std::dynamic_pointer_cast<alert::KeywordAlertRule>(rule)) {
        proto.set_type(xumj::proto::AlertRuleType::KEYWORD);
        
        auto keywordConfig = proto.mutable_keyword_config();
        keywordConfig->set_field(keywordRule->GetField());
        for (const auto& keyword : keywordRule->GetKeywords()) {
            keywordConfig->add_keywords(keyword);
        }
        keywordConfig->set_match_all(keywordRule->GetMatchAll());
        keywordConfig->set_case_sensitive(keywordRule->GetCaseSensitive());
    }
    
    return proto;
}

// 转换Proto到AlertRule
std::shared_ptr<alert::AlertRule> AlertServiceImpl::ConvertRuleFromProto(const xumj::proto::AlertRule& proto) {
    std::shared_ptr<alert::AlertRule> rule;
    
    // 根据规则类型创建不同的规则对象
    switch (proto.type()) {
        case xumj::proto::AlertRuleType::THRESHOLD: {
            if (!proto.has_threshold_config()) {
                return nullptr;
            }
            
            const auto& config = proto.threshold_config();
            rule = std::make_shared<alert::ThresholdAlertRule>(
                proto.name(),
                proto.description(),
                config.field(),
                config.threshold(),
                config.compare_type(),
                static_cast<alert::AlertLevel>(proto.level())
            );
            break;
        }
        
        case xumj::proto::AlertRuleType::KEYWORD: {
            if (!proto.has_keyword_config()) {
                return nullptr;
            }
            
            const auto& config = proto.keyword_config();
            std::vector<std::string> keywords;
            for (int i = 0; i < config.keywords_size(); ++i) {
                keywords.push_back(config.keywords(i));
            }
            
            rule = std::make_shared<alert::KeywordAlertRule>(
                proto.name(),
                proto.description(),
                config.field(),
                keywords,
                config.match_all(),
                static_cast<alert::AlertLevel>(proto.level())
            );
            
            auto keywordRule = std::dynamic_pointer_cast<alert::KeywordAlertRule>(rule);
            if (keywordRule) {
                keywordRule->SetCaseSensitive(config.case_sensitive());
            }
            
            break;
        }
        
        default:
            return nullptr;
    }
    
    // 设置规则ID
    if (!proto.id().empty()) {
        rule->SetId(proto.id());
    }
    
    return rule;
}

// 检查告警是否匹配订阅过滤器
bool AlertServiceImpl::MatchesFilter(
    const xumj::proto::Alert& alert,
    const ::xumj::proto::SubscribeAlertsRequest& filter) {
    
    // 检查级别
    if (filter.levels_size() > 0) {
        bool levelMatched = false;
        for (int i = 0; i < filter.levels_size(); ++i) {
            if (alert.level() == filter.levels(i)) {
                levelMatched = true;
                break;
            }
        }
        
        if (!levelMatched) {
            return false;
        }
    }
    
    // 检查来源
    if (filter.sources_size() > 0) {
        bool sourceMatched = false;
        for (int i = 0; i < filter.sources_size(); ++i) {
            if (alert.source() == filter.sources(i)) {
                sourceMatched = true;
                break;
            }
        }
        
        if (!sourceMatched) {
            return false;
        }
    }
    
    // 检查标签过滤器
    for (const auto& [key, value] : filter.label_filters()) {
        auto it = alert.labels().find(key);
        if (it == alert.labels().end() || it->second != value) {
            return false;
        }
    }
    
    // 检查是否包含已解决的告警
    if (!filter.include_resolved() && alert.status() == xumj::proto::AlertStatus::RESOLVED) {
        return false;
    }
    
    return true;
}

} // namespace grpc
} // namespace xumj 