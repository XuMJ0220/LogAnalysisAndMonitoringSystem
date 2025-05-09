#include "log_service_impl.h"
#include <iostream>
#include <chrono>
#include <sstream>
#include <thread>
#include <zlib.h>

namespace xumj {
namespace grpc {

// 构造函数
LogServiceImpl::LogServiceImpl(
    std::shared_ptr<processor::LogProcessor> processor,
    std::shared_ptr<analyzer::LogAnalyzer> analyzer,
    std::shared_ptr<storage::StorageFactory> storageFactory)
    : processor_(processor), analyzer_(analyzer), storageFactory_(storageFactory) {
    
    // 获取默认存储
    storage_ = storageFactory->GetStorage("mysql");
    if (!storage_) {
        std::cerr << "无法获取MySQL存储，尝试Redis" << std::endl;
        storage_ = storageFactory->GetStorage("redis");
    }
    
    if (!storage_) {
        throw std::runtime_error("无法获取存储引擎");
    }
}

// 提交日志
::grpc::Status LogServiceImpl::SubmitLogs(
    ::grpc::ServerContext* context,
    const ::xumj::proto::LogBatch* request,
    ::xumj::proto::SubmitResponse* response) {
    
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "请求为空");
    }
    
    response->set_batch_id(request->batch_id());
    response->set_success(true);
    
    int acceptedCount = 0;
    int rejectedCount = 0;
    
    // 处理每条日志
    for (const auto& protoLog : request->logs()) {
        try {
            // 转换为内部LogEntry
            collector::LogEntry entry = ConvertFromProto(protoLog);
            
            // 处理日志
            processor_->ProcessLog(entry, [this, &entry](
                const collector::LogEntry& processedEntry,
                const std::unordered_map<std::string, std::string>& parsedFields) {
                
                // 创建日志记录
                analyzer::LogRecord record;
                record.id = processedEntry.id;
                record.timestamp = processedEntry.timestamp;
                record.level = processedEntry.logLevel;
                record.source = processedEntry.source;
                record.message = processedEntry.message;
                record.fields = parsedFields;
                
                // 分析日志
                analyzer_->AnalyzeLog(record, [this, &record](
                    const analyzer::LogRecord& analyzedRecord,
                    const std::unordered_map<std::string, std::string>& results) {
                    
                    // 存储分析结果
                    if (storage_) {
                        storage::Storage::LogEntry storageEntry;
                        storageEntry.id = analyzedRecord.id;
                        storageEntry.timestamp = analyzedRecord.timestamp;
                        storageEntry.level = analyzedRecord.level;
                        storageEntry.source = analyzedRecord.source;
                        storageEntry.message = analyzedRecord.message;
                        
                        // 将分析结果添加到字段中
                        for (const auto& [key, value] : results) {
                            storageEntry.fields["analysis." + key] = value;
                        }
                        
                        storage_->SaveLogEntry(storageEntry);
                    }
                    
                    // 通知订阅者
                    xumj::proto::LogEntry notifyEntry = ConvertToProto(entry);
                    NotifySubscribers(notifyEntry);
                });
            });
            
            acceptedCount++;
        } catch (const std::exception& e) {
            rejectedCount++;
            response->add_rejected_ids(protoLog.id());
            std::cerr << "处理日志失败: " << e.what() << std::endl;
        }
    }
    
    response->set_accepted_count(acceptedCount);
    response->set_rejected_count(rejectedCount);
    
    if (rejectedCount > 0) {
        response->set_success(false);
        response->set_message("部分日志处理失败");
    } else {
        response->set_message("所有日志处理成功");
    }
    
    return ::grpc::Status::OK;
}

// 查询日志
::grpc::Status LogServiceImpl::QueryLogs(
    ::grpc::ServerContext* context,
    const ::xumj::proto::LogQuery* request,
    ::xumj::proto::LogQueryResponse* response) {
    
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "请求为空");
    }
    
    if (!storage_) {
        response->set_success(false);
        response->set_message("存储引擎不可用");
        return ::grpc::Status::OK;
    }
    
    // 构建查询条件
    std::unordered_map<std::string, std::string> conditions;
    
    if (!request->start_time().empty()) {
        conditions["timestamp >= "] = request->start_time();
    }
    
    if (!request->end_time().empty()) {
        conditions["timestamp <= "] = request->end_time();
    }
    
    if (!request->log_levels().empty()) {
        std::ostringstream oss;
        oss << "level IN (";
        for (size_t i = 0; i < request->log_levels_size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << "'" << request->log_levels(i) << "'";
        }
        oss << ")";
        conditions[""] = oss.str();
    }
    
    if (!request->sources().empty()) {
        std::ostringstream oss;
        oss << "source IN (";
        for (size_t i = 0; i < request->sources_size(); ++i) {
            if (i > 0) {
                oss << ", ";
            }
            oss << "'" << request->sources(i) << "'";
        }
        oss << ")";
        conditions[""] = oss.str();
    }
    
    if (!request->message_pattern().empty()) {
        conditions["message LIKE "] = "%" + request->message_pattern() + "%";
    }
    
    // 添加字段过滤条件
    for (const auto& [key, value] : request->field_filters()) {
        conditions["fields." + key + " = "] = value;
    }
    
    try {
        // 查询日志
        auto entries = storage_->QueryLogEntries(
            conditions,
            request->limit(),
            request->offset());
        
        // 设置响应
        response->set_success(true);
        response->set_message("查询成功");
        response->set_total_count(entries.size());
        response->set_has_more(entries.size() == request->limit());
        
        // 转换日志条目
        for (const auto& entry : entries) {
            collector::LogEntry logEntry;
            logEntry.id = entry.id;
            logEntry.timestamp = entry.timestamp;
            logEntry.logLevel = entry.level;
            logEntry.source = entry.source;
            logEntry.message = entry.message;
            
            // 添加字段
            for (const auto& [key, value] : entry.fields) {
                logEntry.fields[key] = value;
            }
            
            // 转换为Proto
            auto* protoEntry = response->add_logs();
            *protoEntry = ConvertToProto(logEntry);
        }
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message(std::string("查询失败: ") + e.what());
    }
    
    return ::grpc::Status::OK;
}

// 获取统计信息
::grpc::Status LogServiceImpl::GetStats(
    ::grpc::ServerContext* context,
    const ::xumj::proto::StatsRequest* request,
    ::xumj::proto::StatsResponse* response) {
    
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "请求为空");
    }
    
    if (!storage_) {
        response->set_success(false);
        response->set_message("存储引擎不可用");
        return ::grpc::Status::OK;
    }
    
    try {
        // 构建查询条件
        std::unordered_map<std::string, std::string> conditions;
        
        if (!request->start_time().empty()) {
            conditions["timestamp >= "] = request->start_time();
        }
        
        if (!request->end_time().empty()) {
            conditions["timestamp <= "] = request->end_time();
        }
        
        // 查询统计信息
        std::vector<std::string> groupBy;
        for (const auto& field : request->group_by()) {
            groupBy.push_back(field);
        }
        
        std::vector<std::string> metrics;
        for (const auto& metric : request->metrics()) {
            metrics.push_back(metric);
        }
        
        auto statsResults = storage_->QueryStats(conditions, groupBy, metrics);
        
        // 设置响应
        response->set_success(true);
        response->set_message("查询成功");
        
        // 转换统计结果
        for (const auto& stats : statsResults) {
            auto* statGroup = response->add_stats();
            
            // 添加维度
            for (const auto& [key, value] : stats.dimensions) {
                (*statGroup->mutable_dimensions())[key] = value;
            }
            
            // 添加指标
            for (const auto& [key, value] : stats.metrics) {
                (*statGroup->mutable_metrics())[key] = value;
            }
        }
    } catch (const std::exception& e) {
        response->set_success(false);
        response->set_message(std::string("查询统计信息失败: ") + e.what());
    }
    
    return ::grpc::Status::OK;
}

// 订阅日志流
::grpc::Status LogServiceImpl::SubscribeLogs(
    ::grpc::ServerContext* context,
    const ::xumj::proto::SubscribeRequest* request,
    ::grpc::ServerWriter<::xumj::proto::LogEntry>* writer) {
    
    if (!request) {
        return ::grpc::Status(::grpc::StatusCode::INVALID_ARGUMENT, "请求为空");
    }
    
    // 查询历史日志
    if (request->include_historical() && storage_) {
        // 构建查询条件
        std::unordered_map<std::string, std::string> conditions;
        
        if (!request->log_levels().empty()) {
            std::ostringstream oss;
            oss << "level IN (";
            for (size_t i = 0; i < request->log_levels_size(); ++i) {
                if (i > 0) {
                    oss << ", ";
                }
                oss << "'" << request->log_levels(i) << "'";
            }
            oss << ")";
            conditions[""] = oss.str();
        }
        
        if (!request->sources().empty()) {
            std::ostringstream oss;
            oss << "source IN (";
            for (size_t i = 0; i < request->sources_size(); ++i) {
                if (i > 0) {
                    oss << ", ";
                }
                oss << "'" << request->sources(i) << "'";
            }
            oss << ")";
            conditions[""] = oss.str();
        }
        
        if (!request->message_pattern().empty()) {
            conditions["message LIKE "] = "%" + request->message_pattern() + "%";
        }
        
        // 添加字段过滤条件
        for (const auto& [key, value] : request->field_filters()) {
            conditions["fields." + key + " = "] = value;
        }
        
        try {
            // 查询历史日志
            auto entries = storage_->QueryLogEntries(conditions, 100, 0);
            
            // 发送历史日志
            for (const auto& entry : entries) {
                collector::LogEntry logEntry;
                logEntry.id = entry.id;
                logEntry.timestamp = entry.timestamp;
                logEntry.logLevel = entry.level;
                logEntry.source = entry.source;
                logEntry.message = entry.message;
                
                // 添加字段
                for (const auto& [key, value] : entry.fields) {
                    logEntry.fields[key] = value;
                }
                
                // 转换为Proto
                xumj::proto::LogEntry protoEntry = ConvertToProto(logEntry);
                
                // 发送日志
                if (!writer->Write(protoEntry)) {
                    return ::grpc::Status(::grpc::StatusCode::ABORTED, "写入流失败");
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "查询历史日志失败: " << e.what() << std::endl;
        }
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
            [writer](const xumj::proto::LogEntry& logEntry) {
                writer->Write(logEntry);
            },
            *request
        });
    }
    
    std::cout << "客户端 " << clientId << " 已订阅日志流" << std::endl;
    
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
void LogServiceImpl::NotifySubscribers(const xumj::proto::LogEntry& logEntry) {
    std::lock_guard<std::mutex> lock(subscribersMutex_);
    
    for (const auto& subscriber : subscribers_) {
        if (MatchesFilter(logEntry, subscriber.filter)) {
            try {
                subscriber.callback(logEntry);
            } catch (const std::exception& e) {
                std::cerr << "通知订阅者失败: " << e.what() << std::endl;
            }
        }
    }
}

// 转换LogEntry到Proto
xumj::proto::LogEntry LogServiceImpl::ConvertToProto(const collector::LogEntry& entry) {
    xumj::proto::LogEntry proto;
    
    proto.set_id(entry.id);
    proto.set_timestamp(entry.timestamp);
    proto.set_log_level(entry.logLevel);
    proto.set_source(entry.source);
    proto.set_message(entry.message);
    
    // 添加字段
    for (const auto& [key, value] : entry.fields) {
        (*proto.mutable_fields())[key] = value;
    }
    
    // 设置压缩标志
    proto.set_compressed(entry.compressed);
    
    // 如果有原始数据，设置
    if (!entry.rawData.empty()) {
        proto.set_raw_data(entry.rawData);
    }
    
    return proto;
}

// 转换Proto到LogEntry
collector::LogEntry LogServiceImpl::ConvertFromProto(const xumj::proto::LogEntry& proto) {
    collector::LogEntry entry;
    
    entry.id = proto.id();
    entry.timestamp = proto.timestamp();
    entry.logLevel = proto.log_level();
    entry.source = proto.source();
    entry.message = proto.message();
    
    // 添加字段
    for (const auto& [key, value] : proto.fields()) {
        entry.fields[key] = value;
    }
    
    // 设置压缩标志
    entry.compressed = proto.compressed();
    
    // 如果有原始数据，设置
    if (proto.has_raw_data()) {
        entry.rawData = proto.raw_data();
    }
    
    return entry;
}

// 检查日志是否匹配订阅过滤器
bool LogServiceImpl::MatchesFilter(
    const xumj::proto::LogEntry& logEntry,
    const ::xumj::proto::SubscribeRequest& filter) {
    
    // 检查日志级别
    if (filter.log_levels_size() > 0) {
        bool levelMatched = false;
        for (const auto& level : filter.log_levels()) {
            if (logEntry.log_level() == level) {
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
        for (const auto& source : filter.sources()) {
            if (logEntry.source() == source) {
                sourceMatched = true;
                break;
            }
        }
        
        if (!sourceMatched) {
            return false;
        }
    }
    
    // 检查消息模式
    if (!filter.message_pattern().empty()) {
        if (logEntry.message().find(filter.message_pattern()) == std::string::npos) {
            return false;
        }
    }
    
    // 检查字段过滤器
    for (const auto& [key, value] : filter.field_filters()) {
        auto it = logEntry.fields().find(key);
        if (it == logEntry.fields().end() || it->second != value) {
            return false;
        }
    }
    
    return true;
}

} // namespace grpc
} // namespace xumj 