#include "xumj/collector/log_collector.h"
#include <algorithm>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <zlib.h>
#include <optional>
#include <memory>
#include <fstream>
#include <thread>

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

// 声明外部推送回调（由collector_server.cpp实现并注册）
LogPushCallback g_logPushCallback = nullptr;
uint64_t g_logPushConnId = 0;
void RegisterLogPushCallback(LogPushCallback cb, uint64_t connId) { g_logPushCallback = cb; g_logPushConnId = connId; }

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
    try {
        size_t log_size = logs.size();
        if (g_logPushCallback) g_logPushCallback(g_logPushConnId, logs);
        for (auto it = logs.begin(); it != logs.end();) {
            it = logs.erase(it);
        }
        if (sendCallback_) {
            sendCallback_(log_size -  logs.size());
        }
        return true;
    } catch (const std::exception& e) {
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

bool LogCollector::CollectFromFile(const std::string& filePath, LogLevel level, size_t intervalMs, int maxLinesPerRound) {
    // 启动一个线程定时读取文件内容
    std::thread([this, filePath, level, intervalMs, maxLinesPerRound]() {
        std::ifstream file(filePath, std::ios::in);
        if (!file.is_open()) {
            if (errorCallback_) errorCallback_("无法打开日志文件: " + filePath);
            return;
        }
        // 启动时从头读取
        file.seekg(0, std::ios::beg);
        std::streampos lastPos = file.tellg();
        if (lastPos == -1) lastPos = 0;
        lastCleanPos_ = lastPos;
        StartCleanThread(filePath); // 启动定时清理线程（可选，保留备份功能）
        while (isActive_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(intervalMs));
            file.clear(); // 清除eof标志
            file.seekg(0, std::ios::end);
            std::streampos endPos = file.tellg();
            bool hasNewLog = (endPos > lastPos);
            std::cout << "[DEBUG] 循环: lastPos=" << lastPos << ", endPos=" << endPos << ", hasNewLog=" << hasNewLog << std::endl;
            if (endPos > lastPos) {
                file.seekg(lastPos);
                std::string line;
                int linesCollected = 0;
                std::streampos lastReadPos = lastPos;
                while (std::getline(file, line)) {
                    if (!line.empty()) {
                        SubmitLog(line, level);
                        linesCollected++;
                    }
                    // 每采集一条就更新lastReadPos
                    std::streampos curPos = file.tellg();
                    if (curPos != -1) {
                        lastReadPos = curPos;
                    } else {
                        lastReadPos += static_cast<std::streampos>(line.size() + 1); // +1 for '\n'
                    }
                    if (linesCollected >= maxLinesPerRound) {
                        std::cout << "[DEBUG] 达到本轮采集上限: " << maxLinesPerRound << " 条，提前结束本轮采集。" << std::endl;
                        break;
                    }
                }
                std::cout << "[DEBUG] 采集后 lastReadPos=" << lastReadPos << ", 本轮采集条数=" << linesCollected << std::endl;
                // 采集完新内容后立即清理已采集内容
                if (hasNewLog && lastReadPos > lastPos && linesCollected > 0) {
                    std::cout << "[DEBUG] 采集到新内容，准备清理已采集内容，lastPos=" << lastPos << ", lastReadPos=" << lastReadPos << std::endl;
                    file.close();
                    // 读取未采集内容
                    std::ifstream in(filePath, std::ios::binary);
                    in.seekg(lastReadPos);
                    std::vector<char> remain((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                    std::cout << "[DEBUG] 未采集内容字节数: " << remain.size() << std::endl;
                    in.close();
                    // 清空并写回未采集内容
                    std::ofstream out(filePath, std::ios::binary | std::ios::trunc);
                    if (!remain.empty()) out.write(remain.data(), remain.size());
                    out.close();
                    std::cout << "[DEBUG] 清理后已写回未采集内容，重新打开文件继续采集。" << std::endl;
                    // 检查清理后文件大小
                    std::ifstream check(filePath, std::ios::binary | std::ios::ate);
                    std::cout << "[DEBUG] 清理后文件大小: " << check.tellg() << std::endl;
                    check.close();
                    // 重新打开文件以继续采集
                    file.open(filePath, std::ios::in);
                    lastPos = 0;
                } else {
                    lastPos = lastReadPos;
                }
            }
        }
    }).detach();
    return true;
}

void LogCollector::StartCleanThread(const std::string& filePath) {
    std::thread([this, filePath]() {
        while (isActive_) {
            std::this_thread::sleep_for(std::chrono::seconds(config_.clean_interval_sec));
            CleanAndBackup(filePath);
        }
    }).detach();
}

void LogCollector::CleanAndBackup(const std::string& filePath) {
    std::lock_guard<std::mutex> lock(fileCleanMutex_);
    std::ifstream file(filePath, std::ios::in | std::ios::binary);
    if (!file.is_open()) return;
    if (lastCleanPos_ <= 0) return;
    // 读取已消费内容
    std::vector<char> consumed(static_cast<size_t>(lastCleanPos_));
    file.read(consumed.data(), lastCleanPos_);
    // 读取未消费内容
    file.seekg(lastCleanPos_);
    std::vector<char> remain((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
    file.close();
    // 备份
    if (config_.enable_backup && !consumed.empty()) {
        auto t = std::time(nullptr);
        char buf[64];
        std::strftime(buf, sizeof(buf), ".bak.%Y%m%d_%H%M%S", std::localtime(&t));
        std::string backupFile = filePath + buf;
        std::ofstream bak(backupFile, std::ios::out | std::ios::binary);
        bak.write(consumed.data(), consumed.size());
        bak.close();
    }
    // 清理（重写未消费内容）
    std::ofstream out(filePath, std::ios::out | std::ios::binary | std::ios::trunc);
    if (!remain.empty()) out.write(remain.data(), remain.size());
    out.close();
    // 更新lastCleanPos_
    lastCleanPos_ = 0;
}

} // namespace collector
} // namespace xumj 