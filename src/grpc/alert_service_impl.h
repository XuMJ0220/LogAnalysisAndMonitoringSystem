#ifndef XUMJ_GRPC_ALERT_SERVICE_IMPL_H
#define XUMJ_GRPC_ALERT_SERVICE_IMPL_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include "alert_service.pb.h"
#include "alert_service.grpc.pb.h"
#include "xumj/alert/alert_manager.h"

namespace xumj {
namespace grpc {

/**
 * @class AlertServiceImpl
 * @brief 告警服务的gRPC实现
 */
class AlertServiceImpl final : public xumj::proto::AlertService::Service {
public:
    /**
     * @brief 构造函数
     * @param alertManager 告警管理器
     */
    explicit AlertServiceImpl(std::shared_ptr<alert::AlertManager> alertManager);
    
    /**
     * @brief 获取告警列表
     * @param context gRPC上下文
     * @param request 请求
     * @param response 响应
     * @return gRPC状态
     */
    ::grpc::Status GetAlerts(
        ::grpc::ServerContext* context,
        const ::xumj::proto::AlertQuery* request,
        ::xumj::proto::AlertQueryResponse* response) override;
    
    /**
     * @brief 获取告警详情
     * @param context gRPC上下文
     * @param request 请求
     * @param response 响应
     * @return gRPC状态
     */
    ::grpc::Status GetAlertDetail(
        ::grpc::ServerContext* context,
        const ::xumj::proto::AlertDetailRequest* request,
        ::xumj::proto::Alert* response) override;
    
    /**
     * @brief 更新告警状态
     * @param context gRPC上下文
     * @param request 请求
     * @param response 响应
     * @return gRPC状态
     */
    ::grpc::Status UpdateAlertStatus(
        ::grpc::ServerContext* context,
        const ::xumj::proto::UpdateAlertStatusRequest* request,
        ::xumj::proto::UpdateAlertStatusResponse* response) override;
    
    /**
     * @brief 创建告警规则
     * @param context gRPC上下文
     * @param request 请求
     * @param response 响应
     * @return gRPC状态
     */
    ::grpc::Status CreateAlertRule(
        ::grpc::ServerContext* context,
        const ::xumj::proto::AlertRule* request,
        ::xumj::proto::CreateAlertRuleResponse* response) override;
    
    /**
     * @brief 更新告警规则
     * @param context gRPC上下文
     * @param request 请求
     * @param response 响应
     * @return gRPC状态
     */
    ::grpc::Status UpdateAlertRule(
        ::grpc::ServerContext* context,
        const ::xumj::proto::AlertRule* request,
        ::xumj::proto::UpdateAlertRuleResponse* response) override;
    
    /**
     * @brief 删除告警规则
     * @param context gRPC上下文
     * @param request 请求
     * @param response 响应
     * @return gRPC状态
     */
    ::grpc::Status DeleteAlertRule(
        ::grpc::ServerContext* context,
        const ::xumj::proto::DeleteAlertRuleRequest* request,
        ::xumj::proto::DeleteAlertRuleResponse* response) override;
    
    /**
     * @brief 获取告警规则列表
     * @param context gRPC上下文
     * @param request 请求
     * @param response 响应
     * @return gRPC状态
     */
    ::grpc::Status GetAlertRules(
        ::grpc::ServerContext* context,
        const ::xumj::proto::AlertRuleQuery* request,
        ::xumj::proto::AlertRuleQueryResponse* response) override;
    
    /**
     * @brief 订阅告警
     * @param context gRPC上下文
     * @param request 请求
     * @param writer 告警流写入器
     * @return gRPC状态
     */
    ::grpc::Status SubscribeAlerts(
        ::grpc::ServerContext* context,
        const ::xumj::proto::SubscribeAlertsRequest* request,
        ::grpc::ServerWriter<::xumj::proto::Alert>* writer) override;

private:
    // 告警管理器
    std::shared_ptr<alert::AlertManager> alertManager_;
    
    // 订阅者管理
    struct Subscriber {
        std::string clientId;
        std::function<void(const xumj::proto::Alert&)> callback;
        ::xumj::proto::SubscribeAlertsRequest filter;
    };
    
    std::vector<Subscriber> subscribers_;
    std::mutex subscribersMutex_;
    
    // 辅助方法：通知所有订阅者
    void NotifySubscribers(const xumj::proto::Alert& alert);
    
    // 辅助方法：转换Alert到Proto
    xumj::proto::Alert ConvertToProto(const alert::Alert& alert);
    
    // 辅助方法：转换Proto到Alert
    alert::Alert ConvertFromProto(const xumj::proto::Alert& proto);
    
    // 辅助方法：转换AlertRule到Proto
    xumj::proto::AlertRule ConvertRuleToProto(const std::shared_ptr<alert::AlertRule>& rule);
    
    // 辅助方法：转换Proto到AlertRule
    std::shared_ptr<alert::AlertRule> ConvertRuleFromProto(const xumj::proto::AlertRule& proto);
    
    // 辅助方法：检查告警是否匹配订阅过滤器
    bool MatchesFilter(
        const xumj::proto::Alert& alert,
        const ::xumj::proto::SubscribeAlertsRequest& filter);
};

} // namespace grpc
} // namespace xumj

#endif // XUMJ_GRPC_ALERT_SERVICE_IMPL_H 