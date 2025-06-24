#include "xumj/collector/log_collector.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <zlib.h>
#include <optional>
#include <memory>

namespace xumj {
namespace collector {

// 辅助函数：将日志级别转换为字符串
std::string LogLevelToString(LogLevel level) {
    switch (level) {
        case LogLevel::TRACE: return "TRACE";
        case LogLevel::DEBUG: return "DEBUG";
        case LogLevel::INFO: return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR: return "ERROR";
        case LogLevel::CRITICAL: return "CRITICAL";
        default: return "UNKNOWN";
    }
}

// 辅助函数：将时间戳转换为格式化字符串
std::string TimestampToString(const std::chrono::system_clock::time_point& timestamp) {
    std::time_t time = std::chrono::system_clock::to_time_t(timestamp);
    std::stringstream ss;
    ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
    return ss.str();
}

// 辅助函数：使用zlib压缩字符串
std::string CompressString(const std::string& data) {
    // 如果输入数据为空，直接返回空字符串
    if (data.empty()) {
        return "";
    }
    
    // 计算压缩后的最大可能大小
    uLong compressedSize = compressBound(data.size());
    std::vector<Bytef> buffer(compressedSize);
    
    // 压缩数据
    int result = compress(buffer.data(), &compressedSize, 
                         reinterpret_cast<const Bytef*>(data.data()), data.size());
    
    // 检查压缩结果
    if (result != Z_OK) {
        return data;  // 压缩失败，返回原始数据
    }
    
    // 返回压缩后的数据
    return std::string(reinterpret_cast<char*>(buffer.data()), compressedSize);
}

LogCollector::LogCollector() : isActive_(false) {
    // 默认构造函数，需要后续调用Initialize进行初始化
}

LogCollector::LogCollector(const CollectorConfig& config) : isActive_(false) {
    Initialize(config);
}

LogCollector::~LogCollector() {
    Shutdown();
}

bool LogCollector::Initialize(const CollectorConfig& config) {
    // 如果收集器已经处于活动状态，先关闭它
    if (isActive_) {
        Shutdown();
    }
    
    // 保存配置
    config_ = config;
    
    // 初始化内存池
    memoryPool_ = std::make_unique<common::MemoryPool>(
        sizeof(LogEntry), config_.memoryPoolSize);
    
    // 初始化线程池
    threadPool_ = std::make_unique<common::ThreadPool>(
        config_.threadPoolSize);
    
    // 添加默认级别过滤器
    AddFilter(std::make_shared<LevelFilter>(config_.minLevel));
    
    // 启动刷新线程
    isActive_ = true;
    flushThread_ = std::thread(&LogCollector::FlushThreadFunc, this);
    
    return true;
}

bool LogCollector::SubmitLog(const std::string& logContent, LogLevel level) {
    // 检查收集器状态
    if (!isActive_) {
        if (errorCallback_) {
            errorCallback_("Collector is not active");
        }
        return false;
    }
    
    // 创建日志条目
    LogEntry entry(config_.compressLogs ? CompressLogContent(logContent) : logContent, level);
    
    // 应用过滤规则
    if (ShouldFilterLog(entry)) {
        return true;  // 被过滤的日志视为成功处理
    }
    
    // 将日志添加到队列
    logQueue_.Push(std::move(entry));
    
    // 如果队列已满，触发刷新
    if (GetPendingCount() >= config_.maxQueueSize) {
        threadPool_->Submit([this]() { this->Flush(); });
    }
    
    return true;
}

bool LogCollector::SubmitLogs(const std::vector<std::string>& logContents, LogLevel level) {
    // 检查收集器状态
    if (!isActive_) {
        if (errorCallback_) {
            errorCallback_("Collector is not active");
        }
        return false;
    }
    
    // 批量处理日志
    bool allSucceeded = true;
    for (const auto& content : logContents) {
        if (!SubmitLog(content, level)) {
            allSucceeded = false;
        }
    }
    
    return allSucceeded;
}

void LogCollector::AddFilter(std::shared_ptr<LogFilterInterface> filter) {
    if (filter) {
        std::lock_guard<std::mutex> lock(filtersMutex_);
        filters_.push_back(std::move(filter));
    }
}

void LogCollector::ClearFilters() {
    std::lock_guard<std::mutex> lock(filtersMutex_);
    filters_.clear();
}

void LogCollector::Flush() {
    if (!isActive_) return;
    
    std::vector<LogEntry> batch;
    batch.reserve(config_.batchSize);
    
    // 从队列中获取一批日志
    for (size_t i = 0; i < config_.batchSize; ++i) {
        if (auto entry = logQueue_.Pop()) {
            batch.push_back(std::move(*entry));
        } else {
            break;  // 队列为空
        }
    }
    
    // 如果获取到了日志，发送它们
    if (!batch.empty()) {
        if (!SendLogBatch(batch) && config_.enableRetry) {
            HandleRetry(batch);
        }
    }
}

void LogCollector::Shutdown() {
    // 设置状态为非活动
    isActive_ = false;
    
    // 等待刷新线程结束
    if (flushThread_.joinable()) {
        flushThread_.join();
    }
    
    // 刷新所有剩余的日志
    Flush();
    
    // 清理资源
    threadPool_.reset();
    memoryPool_.reset();
}

size_t LogCollector::GetPendingCount() const {
    // 返回当前未处理的日志数量（精确值）
    return logQueue_.Size();
}

void LogCollector::SetSendCallback(std::function<void(size_t)> callback) {
    sendCallback_ = std::move(callback);
}

void LogCollector::SetErrorCallback(std::function<void(const std::string&)> callback) {
    errorCallback_ = std::move(callback);
}

bool LogCollector::SendLogBatch(std::vector<LogEntry>& logs) {
    // 实际实现中，这里会通过网络发送日志到服务器
    // 在此示例中，我们只是简单地打印日志
    
    try {
        size_t log_size = logs.size();
        for (auto it = logs.begin(); it != logs.end();) {
            // std::cout << "[" << TimestampToString(it->GetTimestamp()) << "] "
            //           << "[" << LogLevelToString(it->GetLevel()) << "] "
            //           << it->GetContent() << std::endl;
            it = logs.erase(it); // 删除当前元素并更新迭代器
        }
        
        // 调用成功回调
        if (sendCallback_) {
            sendCallback_(log_size -  logs.size());
        }
        
        return true;
    } catch (const std::exception& e) {
        // 发生错误
        if (errorCallback_) {
            errorCallback_(std::string("Failed to send logs: ") + e.what());
        }
        return false;
    }
}

void LogCollector::FlushThreadFunc() {
    while (isActive_) {
        // 定期刷新日志
        std::this_thread::sleep_for(config_.flushInterval);
        if (isActive_) {  // 再次检查，防止在休眠期间收集器被关闭
            Flush();
        }
    }
}

bool LogCollector::ShouldFilterLog(const LogEntry& entry) const {
    std::lock_guard<std::mutex> lock(filtersMutex_);
    
    // 应用所有过滤器
    for (const auto& filter : filters_) {
        if (filter->ShouldFilter(entry)) {
            return true;  // 如果任何过滤器返回true，则过滤该日志
        }
    }
    
    return false;  // 没有过滤器过滤该日志
}

void LogCollector::HandleRetry(const std::vector<LogEntry>& logs) {
    // 在另一个线程中处理重试逻辑
    threadPool_->Submit([this, logs]() {
        for (uint32_t attempt = 0; attempt < config_.maxRetryCount; ++attempt) {
            // 等待重试间隔
            std::this_thread::sleep_for(config_.retryInterval);
            
            // 如果收集器已关闭，停止重试
            if (!isActive_) {
                break;
            }
            
            // 尝试重新发送
            std::vector<LogEntry> retryLogs = logs;  // 创建副本
            if (SendLogBatch(retryLogs)) {
                return;  // 发送成功，结束重试
            }
        }
        
        // 达到最大重试次数，记录错误
        if (errorCallback_) {
            errorCallback_("Failed to send logs after maximum retry attempts");
        }
    });
}

std::string LogCollector::CompressLogContent(const std::string& content) {
    return CompressString(content);
}

} // namespace collector
} // namespace xumj 