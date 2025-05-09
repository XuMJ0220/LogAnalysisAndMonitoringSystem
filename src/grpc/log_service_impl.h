#ifndef XUMJ_GRPC_LOG_SERVICE_IMPL_H
#define XUMJ_GRPC_LOG_SERVICE_IMPL_H

#include <memory>
#include <string>
#include <vector>
#include <unordered_map>
#include <mutex>
#include <grpcpp/grpcpp.h>
#include "log_service.pb.h"
#include "log_service.grpc.pb.h"
#include "xumj/processor/log_processor.h"
#include "xumj/analyzer/log_analyzer.h"
#include "xumj/storage/storage_factory.h"

namespace xumj {
namespace grpc {

/**
 * @class LogServiceImpl
 * @brief 日志服务的gRPC实现
 */
class LogServiceImpl final : public xumj::proto::LogService::Service {
public:
    /**
     * @brief 构造函数
     * @param processor 日志处理器
     * @param analyzer 日志分析器
     * @param storageFactory 存储工厂
     */
    LogServiceImpl(
        std::shared_ptr<processor::LogProcessor> processor,
        std::shared_ptr<analyzer::LogAnalyzer> analyzer,
        std::shared_ptr<storage::StorageFactory> storageFactory);
    
    /**
     * @brief 提交日志
     * @param context gRPC上下文
     * @param request 请求
     * @param response 响应
     * @return gRPC状态
     */
    ::grpc::Status SubmitLogs(
        ::grpc::ServerContext* context,
        const ::xumj::proto::LogBatch* request,
        ::xumj::proto::SubmitResponse* response) override;
    
    /**
     * @brief 查询日志
     * @param context gRPC上下文
     * @param request 请求
     * @param response 响应
     * @return gRPC状态
     */
    ::grpc::Status QueryLogs(
        ::grpc::ServerContext* context,
        const ::xumj::proto::LogQuery* request,
        ::xumj::proto::LogQueryResponse* response) override;
    
    /**
     * @brief 获取统计信息
     * @param context gRPC上下文
     * @param request 请求
     * @param response 响应
     * @return gRPC状态
     */
    ::grpc::Status GetStats(
        ::grpc::ServerContext* context,
        const ::xumj::proto::StatsRequest* request,
        ::xumj::proto::StatsResponse* response) override;
    
    /**
     * @brief 订阅日志流
     * @param context gRPC上下文
     * @param request 请求
     * @param writer 日志流写入器
     * @return gRPC状态
     */
    ::grpc::Status SubscribeLogs(
        ::grpc::ServerContext* context,
        const ::xumj::proto::SubscribeRequest* request,
        ::grpc::ServerWriter<::xumj::proto::LogEntry>* writer) override;

private:
    // 日志处理器
    std::shared_ptr<processor::LogProcessor> processor_;
    
    // 日志分析器
    std::shared_ptr<analyzer::LogAnalyzer> analyzer_;
    
    // 存储工厂
    std::shared_ptr<storage::StorageFactory> storageFactory_;
    
    // 存储引擎
    std::shared_ptr<storage::Storage> storage_;
    
    // 订阅者管理
    struct Subscriber {
        std::string clientId;
        std::function<void(const xumj::proto::LogEntry&)> callback;
        ::xumj::proto::SubscribeRequest filter;
    };
    
    std::vector<Subscriber> subscribers_;
    std::mutex subscribersMutex_;
    
    // 辅助方法：通知所有订阅者
    void NotifySubscribers(const xumj::proto::LogEntry& logEntry);
    
    // 辅助方法：转换LogEntry到Proto
    xumj::proto::LogEntry ConvertToProto(const collector::LogEntry& entry);
    
    // 辅助方法：转换Proto到LogEntry
    collector::LogEntry ConvertFromProto(const xumj::proto::LogEntry& proto);
    
    // 辅助方法：检查日志是否匹配订阅过滤器
    bool MatchesFilter(
        const xumj::proto::LogEntry& logEntry,
        const ::xumj::proto::SubscribeRequest& filter);
};

} // namespace grpc
} // namespace xumj

#endif // XUMJ_GRPC_LOG_SERVICE_IMPL_H 