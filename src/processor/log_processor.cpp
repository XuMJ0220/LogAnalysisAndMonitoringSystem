#include "xumj/processor/log_processor.h"
#include "xumj/storage/storage_factory.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <iomanip>
#include <nlohmann/json.hpp>
#include <zlib.h>
#include <uuid/uuid.h>

namespace xumj {
namespace processor {

// 生成UUID
std::string GenerateUUID() {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    return std::string(uuid_str);
}

// 时间戳转字符串
std::string TimestampToString(const std::chrono::system_clock::time_point& tp) {
    auto time_t_now = std::chrono::system_clock::to_time_t(tp);
    std::tm tm_now = *std::localtime(&time_t_now);
    char buffer[30];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_now);
    return std::string(buffer);
}

// JsonLogParser实现
bool JsonLogParser::Parse(const LogData& logData, analyzer::LogRecord& record) {
    using json = nlohmann::json;
    
    std::cout << "JsonLogParser: 尝试解析日志数据，ID=" << logData.id << std::endl;
    std::cout << "  源: " << logData.source << std::endl;
    std::cout << "  消息内容: " << (logData.message.length() > 50 ? logData.message.substr(0, 47) + "..." : logData.message) << std::endl;
    std::cout << "  元数据数量: " << logData.metadata.size() << std::endl;
    
    // 设置基本字段
    record.id = logData.id;
    record.timestamp = TimestampToString(logData.timestamp);
    record.source = logData.source;
    
    // 检查元数据中是否已经有预处理的JSON字段
    bool hasMetaTimestamp = logData.metadata.count("timestamp") > 0;
    bool hasMetaLevel = logData.metadata.count("level") > 0;
    bool hasMetaMessage = logData.metadata.count("message") > 0;
    bool isJsonFormat = logData.metadata.count("is_json") > 0 && logData.metadata.at("is_json") == "true";
    
    if (hasMetaTimestamp) {
        record.timestamp = logData.metadata.at("timestamp");
        std::cout << "  使用元数据中的时间戳: " << record.timestamp << std::endl;
    }
    
    if (hasMetaLevel) {
        record.level = logData.metadata.at("level");
        std::cout << "  使用元数据中的级别: " << record.level << std::endl;
    }
    
    // 获取日志内容
    const std::string& content = logData.message;
    
    try {
        // 解析JSON
        if (!isJsonFormat) {
            std::cout << "  不是JSON格式，跳过解析" << std::endl;
            return false;
        }
        
        std::cout << "  尝试解析JSON: " << (content.length() > 50 ? content.substr(0, 47) + "..." : content) << std::endl;
        json j = json::parse(content);
        std::cout << "  JSON解析成功" << std::endl;
        
        // 提取常见字段
        if (j.contains("timestamp") && j["timestamp"].is_string() && !hasMetaTimestamp) {
            record.timestamp = j["timestamp"].get<std::string>();
            std::cout << "  从JSON中提取时间戳: " << record.timestamp << std::endl;
        }
        
        if (j.contains("level") && j["level"].is_string() && !hasMetaLevel) {
            record.level = j["level"].get<std::string>();
            std::cout << "  从JSON中提取级别: " << record.level << std::endl;
        }
        
        if (j.contains("message") && j["message"].is_string() && !hasMetaMessage) {
            record.message = j["message"].get<std::string>();
            std::cout << "  从JSON中提取消息: " << record.message << std::endl;
        }
        
        if (j.contains("source") && j["source"].is_string() && record.source.empty()) {
            record.source = j["source"].get<std::string>();
            std::cout << "  从JSON中提取源: " << record.source << std::endl;
        }
        
        // 确保基本字段存在
        if (record.level.empty()) {
            record.level = "INFO";  // 默认级别
            std::cout << "  级别为空，设置默认级别: INFO" << std::endl;
        }
        
        if (record.message.empty()) {
            if (hasMetaMessage) {
                record.message = logData.metadata.at("message");
                std::cout << "  使用元数据中的消息: " << (record.message.length() > 50 ? record.message.substr(0, 47) + "..." : record.message) << std::endl;
            } else {
                // 如果消息为空，使用内容概要作为消息
                record.message = "JSON日志: " + (content.size() > 50 ? content.substr(0, 50) + "..." : content);
                std::cout << "  消息为空，使用内容概要: " << (record.message.length() > 50 ? record.message.substr(0, 47) + "..." : record.message) << std::endl;
            }
        }
        
        // 添加所有其他的JSON字段作为记录字段
        for (auto it = j.begin(); it != j.end(); ++it) {
            if (it.key() != "timestamp" && it.key() != "level" && 
                it.key() != "message" && it.key() != "source") {
                try {
                    std::string fieldName = "json." + it.key();
                    if (it.value().is_string()) {
                        record.fields[fieldName] = it.value().get<std::string>();
                        std::cout << "  添加字段 " << fieldName << " = " << (record.fields[fieldName].length() > 30 ? record.fields[fieldName].substr(0, 27) + "..." : record.fields[fieldName]) << std::endl;
                    } else {
                        record.fields[fieldName] = it.value().dump();
                        std::cout << "  添加非字符串字段 " << fieldName << " = " << (record.fields[fieldName].length() > 30 ? record.fields[fieldName].substr(0, 27) + "..." : record.fields[fieldName]) << std::endl;
                    }
                } catch (...) {
                    record.fields["json." + it.key()] = "无法转换的值类型";
                    std::cout << "  添加字段时出错 " << "json." + it.key() << " = 无法转换的值类型" << std::endl;
                }
            }
        }
        
        std::cout << "JsonLogParser: 解析成功" << std::endl;
        return true;
    } catch (const json::exception& e) {
        // 处理JSON解析错误
        std::cout << "  JSON解析异常: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        // 处理其他解析错误
        std::cout << "  解析异常: " << e.what() << std::endl;
        return false;
    }
}

// LogProcessor实现
LogProcessor::LogProcessor(const LogProcessorConfig& config)
    : config_(config),
      running_(false),
      redisStorage_(nullptr),
      mysqlStorage_(nullptr),
      analyzer_(nullptr) {
    
    // 初始化存储
    if (config_.enableRedisStorage) {
        try {
            auto redisConfig = storage::RedisConfig();
            redisConfig.host = "127.0.0.1";
            redisConfig.port = 6379;
            redisStorage_ = std::make_shared<storage::RedisStorage>(redisConfig);
            std::cout << "Redis存储初始化成功" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Redis存储初始化失败: " << e.what() << std::endl;
        }
    }
    
    if (config_.enableMySQLStorage) {
        try {
            auto mysqlConfig = storage::MySQLConfig();
            mysqlConfig.host = "127.0.0.1";
            mysqlConfig.port = 3306;
            mysqlConfig.username = "root";
            mysqlConfig.password = "ytfhqqkso1";
            mysqlConfig.database = "log_analysis";
            mysqlStorage_ = std::make_shared<storage::MySQLStorage>(mysqlConfig);
            if (mysqlStorage_->Initialize()) {
                std::cout << "MySQL存储初始化成功" << std::endl;
            } else {
                std::cerr << "MySQL存储表结构初始化失败" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "MySQL存储初始化失败: " << e.what() << std::endl;
        }
    }
    
    // 注册默认解析器
    AddLogParser(std::make_shared<JsonLogParser>());
}

LogProcessor::~LogProcessor() {
    Stop();
}

bool LogProcessor::Start() {
    if (running_) {
        return true;  // 已经在运行
    }
    
    // 初始化TCP服务器
    if (!tcpServer_) {
        bool success = InitializeTcpServer();
        if (!success) {
            std::cerr << "初始化TCP服务器失败，日志处理器启动失败" << std::endl;
            return false;
        }
        std::cout << "TCP服务器初始化成功" << std::endl;
    }
    
    // 启动分析器
    if (analyzer_) {
        analyzer_->Start();
    }
    
    // 启动工作线程
    running_ = true;
    
    for (int i = 0; i < config_.workerThreads; ++i) {
        workers_.emplace_back(&LogProcessor::WorkerThread, this);
    }
    
    return true;
}

void LogProcessor::Stop() {
    if (!running_) {
        return;  // 已经停止
    }
    
    // 停止处理线程
    running_ = false;
    queueCondVar_.notify_all();
    
    // 等待线程结束
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    workers_.clear();
    
    // 停止分析器
    if (analyzer_) {
        analyzer_->Stop();
    }
    
    // 停止TCP服务器
    if (tcpServer_) {
        tcpServer_->Stop();
    }
    
    // 清空待处理队列
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        std::queue<LogData> empty;
        std::swap(logQueue_, empty);
        dataCount_ = 0;
    }
}

bool LogProcessor::SubmitLogData(const LogData& data) {
    if (!running_) {
        return false;
    }
    
    // 检查队列大小
    if (GetPendingCount() >= static_cast<size_t>(config_.queueSize)) {
        return false;  // 队列已满
    }
    
    // 添加数据到待处理队列
    {
        std::lock_guard<std::mutex> lock(queueMutex_);
        logQueue_.push(data);
        dataCount_++;
    }
    
    // 通知工作线程
    queueCondVar_.notify_one();
    
    return true;
}

void LogProcessor::AddLogParser(std::shared_ptr<LogParser> parser) {
    std::lock_guard<std::mutex> lock(parsersMutex_);
    parsers_.push_back(parser);
}

size_t LogProcessor::GetPendingCount() const {
    std::lock_guard<std::mutex> lock(queueMutex_);
    return dataCount_;
}

void LogProcessor::WorkerThread() {
    while (running_) {
        LogData data;
        bool hasData = false;
        
        // 获取一个任务
        {
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            // 等待任务或停止信号
            queueCondVar_.wait(lock, [this] {
                return !running_ || !logQueue_.empty();
            });
            
            // 检查是否应该退出
            if (!running_ && logQueue_.empty()) {
                break;
            }
            
            // 获取任务
            if (!logQueue_.empty()) {
                data = std::move(logQueue_.front());
                logQueue_.pop();
                dataCount_--;
                hasData = true;
            }
        }
        
        // 处理任务
        if (hasData) {
            ProcessLogData(std::move(data));
        }
    }
}

bool LogProcessor::InitializeTcpServer() {
    try {
        // 创建TCP服务器，监听特定端口
        tcpServer_ = std::make_unique<network::TcpServer>("LogServer", "0.0.0.0", config_.tcpPort, 4);
        
        // 设置消息回调
        tcpServer_->SetMessageCallback([this](uint64_t connectionId, const std::string& message, muduo::Timestamp) {
            // 直接处理消息，无需查找连接
            std::cout << "接收到来自连接 " << connectionId << " 的消息" << std::endl;
            
            // 获取连接对象
            auto conn = tcpServer_->GetConnection(connectionId);
            if (conn) {
                // 使用真实连接对象
                HandleTcpMessage(conn, message);
            } else {
                std::cerr << "警告: 收到消息但找不到对应的连接，connectionId=" << connectionId << std::endl;
                
                // 创建一个临时的LogData，不依赖于连接对象
                LogData logData;
                logData.timestamp = std::chrono::system_clock::now();
                logData.message = message;
                logData.id = GenerateUUID();
                logData.source = "unknown:" + std::to_string(connectionId);
                
                // 检查是否为JSON格式
                bool isJson = false;
                try {
                    if (message.find("{") == 0 && message.rfind("}") == message.length() - 1) {
                        nlohmann::json json = nlohmann::json::parse(message);
                        isJson = true;
                        std::cout << "成功解析JSON消息，来源未知" << std::endl;
                        
                        // 提取基本元数据
                        if (json.contains("timestamp") && json["timestamp"].is_string()) {
                            logData.metadata["timestamp"] = json["timestamp"].get<std::string>();
                        }
                        
                        if (json.contains("level") && json["level"].is_string()) {
                            logData.metadata["level"] = json["level"].get<std::string>();
                        }
                        
                        if (json.contains("message") && json["message"].is_string()) {
                            logData.metadata["message"] = json["message"].get<std::string>();
                        }
                        
                        if (json.contains("source") && json["source"].is_string()) {
                            logData.source = json["source"].get<std::string>();
                        }
                    }
                } catch (...) {
                    std::cerr << "JSON解析失败" << std::endl;
                }
                
                // 设置JSON标记
                logData.metadata["is_json"] = isJson ? "true" : "false";
                
                // 尝试直接保存到MySQL
                if (mysqlStorage_) {
                    try {
                        std::cout << "尝试直接将未知连接的消息保存到MySQL，ID=" << logData.id << std::endl;
                        
                        storage::MySQLStorage::LogEntry directEntry;
                        directEntry.id = "direct-" + logData.id;
                        directEntry.timestamp = TimestampToString(logData.timestamp);
                        directEntry.level = logData.metadata.count("level") > 0 ? logData.metadata.at("level") : "INFO";
                        directEntry.source = logData.source;
                        directEntry.message = message.length() > 1000 ? message.substr(0, 997) + "..." : message;
                        
                        bool saved = mysqlStorage_->SaveLogEntry(directEntry);
                        if (saved) {
                            std::cout << "成功直接保存未知连接的消息到MySQL，ID=" << directEntry.id << std::endl;
                        } else {
                            std::cerr << "直接保存未知连接的消息到MySQL失败" << std::endl;
                        }
                    } catch (const std::exception& e) {
                        std::cerr << "未知连接消息直接保存MySQL异常: " << e.what() << std::endl;
                    }
                }
                
                // 处理日志数据
                ProcessLogData(std::move(logData));
            }
        });
        
        // 设置连接回调
        tcpServer_->SetConnectionCallback(std::bind(&LogProcessor::HandleTcpConnection, this,
                                              std::placeholders::_1,
                                              std::placeholders::_2,
                                              std::placeholders::_3));
        
        // 启动服务器
        tcpServer_->Start();
        return true;
    } catch (const std::exception& e) {
        std::cerr << "初始化TCP服务器失败: " << e.what() << std::endl;
        return false;
    }
}

void LogProcessor::HandleTcpConnection(uint64_t connectionId, const std::string& clientAddr, bool connected) {
    std::cout << "\n========== TCP 连接事件 ==========" << std::endl;
    std::cout << "连接ID: " << connectionId << std::endl;
    std::cout << "客户端地址: " << clientAddr << std::endl;
    std::cout << "状态: " << (connected ? "已连接" : "已断开") << std::endl;
    std::cout << "=================================\n" << std::endl;
    
    if (connected) {
        // 新连接
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_[connectionId] = clientAddr;
    } else {
        // 连接断开
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        connections_.erase(connectionId);
    }
}

void LogProcessor::HandleTcpMessage(const TcpConnectionPtr& conn, const std::string& message) {
    try {
        if (config_.debug) {
            std::cout << "收到TCP消息，连接ID: " << conn->name() << std::endl;
            std::cout << "  消息内容: " << (message.length() > 50 ? message.substr(0, 47) + "..." : message) << std::endl;
        }
        
        if (message.empty()) {
            if (config_.debug) {
                std::cout << "  消息为空，已忽略" << std::endl;
            }
            return;
        }
        
        // 创建日志数据对象
        LogData logData;
        logData.timestamp = std::chrono::system_clock::now();
        logData.message = message;
        logData.id = GenerateUUID();
        
        // 使用连接名称作为源信息
        logData.source = conn->name();
        
        // 直接保存原始消息到MySQL（测试用）
        if (mysqlStorage_) {
            try {
                std::cout << "尝试直接将消息保存到MySQL，ID=" << logData.id << std::endl;
                
                storage::MySQLStorage::LogEntry directEntry;
                directEntry.id = "direct-" + logData.id;
                directEntry.timestamp = TimestampToString(logData.timestamp);
                directEntry.level = "INFO";
                directEntry.source = logData.source;
                directEntry.message = message.length() > 1000 ? message.substr(0, 997) + "..." : message;
                
                bool saved = mysqlStorage_->SaveLogEntry(directEntry);
                if (saved) {
                    std::cout << "成功直接保存消息到MySQL，ID=" << directEntry.id << std::endl;
                } else {
                    std::cerr << "直接保存消息到MySQL失败" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "直接保存MySQL异常: " << e.what() << std::endl;
            }
        }
        
        if (config_.debug) {
            std::cout << "  创建日志数据: ID=" << logData.id << ", 来源=" << logData.source << std::endl;
        }
        
        // 检查是否为JSON格式，并进行预处理
        bool isJson = false;
        try {
            if (message.find("{") == 0 && message.rfind("}") == message.length() - 1) {
                if (config_.debug) {
                    std::cout << "  消息包含花括号，尝试JSON解析" << std::endl;
                }
                
                // 尝试解析为JSON
                nlohmann::json json = nlohmann::json::parse(message);
                isJson = true;
                
                if (config_.debug) {
                    std::cout << "  JSON解析成功" << std::endl;
                }
                
                // 提取元数据
                if (json.contains("timestamp") && json["timestamp"].is_string()) {
                    logData.metadata["timestamp"] = json["timestamp"].get<std::string>();
                    
                    if (config_.debug) {
                        std::cout << "  提取到timestamp: " << logData.metadata["timestamp"] << std::endl;
                    }
                }
                
                if (json.contains("level") && json["level"].is_string()) {
                    logData.metadata["level"] = json["level"].get<std::string>();
                    
                    if (config_.debug) {
                        std::cout << "  提取到level: " << logData.metadata["level"] << std::endl;
                    }
                }
                
                if (json.contains("message") && json["message"].is_string()) {
                    logData.metadata["message"] = json["message"].get<std::string>();
                    
                    if (config_.debug) {
                        std::cout << "  提取到message: " << logData.metadata["message"] << std::endl;
                    }
                }
                
                if (json.contains("type") && json["type"].is_string()) {
                    logData.metadata["type"] = json["type"].get<std::string>();
                    
                    if (config_.debug) {
                        std::cout << "  提取到type: " << logData.metadata["type"] << std::endl;
                    }
                }
                
                if (json.contains("source") && json["source"].is_string()) {
                    logData.metadata["source"] = json["source"].get<std::string>();
                    
                    if (config_.debug) {
                        std::cout << "  提取到source: " << logData.metadata["source"] << std::endl;
                    }
                }
            } else {
                if (config_.debug) {
                    std::cout << "  消息不符合JSON格式（缺少花括号）" << std::endl;
                }
            }
        } catch (const nlohmann::json::exception& e) {
            if (config_.debug) {
                std::cout << "  JSON解析失败: " << e.what() << std::endl;
            }
            // 不是有效的JSON，继续使用原始消息
        }
        
        // 设置JSON标记
        logData.metadata["is_json"] = isJson ? "true" : "false";
        
        if (config_.debug) {
            std::cout << "  设置消息metadata: is_json = " << (isJson ? "true" : "false") << std::endl;
            std::cout << "  最终元数据数量: " << logData.metadata.size() << std::endl;
            for (const auto& [key, value] : logData.metadata) {
                std::cout << "    " << key << " = " << value << std::endl;
            }
        }
        
        // 处理日志数据
        ProcessLogData(std::move(logData));
        
        if (config_.debug) {
            std::cout << "TCP消息处理完成" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "处理TCP消息时出错: " << e.what() << std::endl;
    } catch (...) {
        std::cerr << "处理TCP消息时出现未知错误" << std::endl;
    }
}

void LogProcessor::ProcessLogData(LogData logData) {
    if (config_.debug) {
        std::cout << "开始处理日志数据: ID=" << logData.id << ", 来源=" << logData.source << std::endl;
    }
    
    // 尝试解析日志数据
    bool parsed = false;
    analyzer::LogRecord record;
    
    // 设置基本字段
    record.id = logData.id.empty() ? GenerateUUID() : logData.id;
    record.timestamp = TimestampToString(logData.timestamp);
    record.source = logData.source;
    
    {
        std::lock_guard<std::mutex> lock(parsersMutex_);
        
        if (config_.debug) {
            std::cout << "尝试解析日志: 解析器数量=" << parsers_.size() << std::endl;
        }
        
        for (auto& parser : parsers_) {
            try {
                if (config_.debug) {
                    std::cout << "使用解析器 " << parser->GetType() << " 解析..." << std::endl;
                }
                
                // 尝试解析
                if (parser->Parse(logData, record)) {
                    parsed = true;
                    
                    if (config_.debug) {
                        std::cout << "解析成功，解析器=" << parser->GetType() << std::endl;
                    }
                    
                    break;
                }
            } catch (const std::exception& e) {
                std::cerr << "解析器 " << parser->GetType() << " 解析失败: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "解析器 " << parser->GetType() << " 解析时发生未知异常" << std::endl;
            }
        }
    }
    
    // 如果没有成功解析，使用默认值
    if (!parsed) {
        if (config_.debug) {
            std::cout << "没有合适的解析器，使用默认值" << std::endl;
        }
        
        record.level = "INFO";
        record.message = logData.message.empty() ? "Empty log message" : logData.message;
        
        // 添加原始消息作为字段
        record.fields["raw_message"] = logData.message;
    }
    
    // 确保记录有ID
    if (record.id.empty()) {
        record.id = GenerateUUID();
    }
    
    if (config_.debug) {
        std::cout << "日志记录已准备: ID=" << record.id 
                  << ", 级别=" << record.level
                  << ", 消息=" << (record.message.length() > 30 ? record.message.substr(0, 27) + "..." : record.message)
                  << std::endl;
    }
    
    // 存储到Redis
    if (redisStorage_) {
        try {
            if (config_.debug) {
                std::cout << "存储日志到Redis..." << std::endl;
            }
            
            StoreRedisLog(record);
            
            if (config_.debug) {
                std::cout << "Redis存储完成" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "存储到Redis失败: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "存储到Redis时发生未知异常" << std::endl;
        }
    }
    
    // 存储到MySQL
    if (mysqlStorage_) {
        try {
            if (config_.debug) {
                std::cout << "存储日志到MySQL..." << std::endl;
            }
            
            StoreMySQLLog(record);
            
            if (config_.debug) {
                std::cout << "MySQL存储完成" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "存储到MySQL失败: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "存储到MySQL时发生未知异常" << std::endl;
        }
    }
    
    // 发送到分析器
    if (analyzer_ && parsed) {
        try {
            if (config_.debug) {
                std::cout << "发送日志到分析器..." << std::endl;
            }
            
            analyzer_->SubmitRecord(record);
            
            if (config_.debug) {
                std::cout << "日志已发送到分析器" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "发送到分析器失败: " << e.what() << std::endl;
        } catch (...) {
            std::cerr << "发送到分析器时发生未知异常" << std::endl;
        }
    }
    
    if (config_.debug) {
        std::cout << "日志处理完成: ID=" << record.id << std::endl;
    }
}

void LogProcessor::StoreRedisLog(const analyzer::LogRecord& record) {
    if (!redisStorage_) {
        return;
    }
    
    try {
        // 准备日志数据
        std::string key = "log:" + record.id;
        
        // 保存主要字段
        redisStorage_->HashSet(key, "id", record.id);
        redisStorage_->HashSet(key, "timestamp", record.timestamp);
        redisStorage_->HashSet(key, "level", record.level);
        redisStorage_->HashSet(key, "source", record.source);
        redisStorage_->HashSet(key, "message", record.message);
        
        // 添加自定义字段
        for (const auto& [field, value] : record.fields) {
            redisStorage_->HashSet(key, field, value);
        }
        
        // 添加到索引
        redisStorage_->ListPush("logs", record.id);
        redisStorage_->ListPush("logs:" + record.level, record.id);
        redisStorage_->ListPush("logs:" + record.source, record.id);
        
        // 设置过期时间 (7天)
        redisStorage_->Expire(key, 7 * 24 * 60 * 60);
        
        if (config_.debug) {
            std::cout << "Redis存储成功: 日志ID = " << record.id << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Redis存储日志时出错: " << e.what() << std::endl;
    }
}

void LogProcessor::StoreMySQLLog(const analyzer::LogRecord& record) {
    if (!mysqlStorage_) {
        return;
    }
    
    try {
        // 确保ID不为空
        std::string id = record.id;
        if (id.empty()) {
            id = GenerateUUID();
        }
        
        // 准备时间戳
        std::string timestamp = record.timestamp;
        if (timestamp.empty()) {
            timestamp = TimestampToString(std::chrono::system_clock::now());
        }
        
        // 截断过长的消息
        std::string message = record.message;
        if (message.length() > 1024) {
            message = message.substr(0, 1021) + "...";
        }
        
        // 准备源标识
        std::string source = record.source;
        if (source.length() > 128) {
            source = source.substr(0, 125) + "...";
        }
        
        // 准备级别
        std::string level = record.level;
        if (level.length() > 16) {
            level = level.substr(0, 16);
        }
        
        // 调试输出
        if (config_.debug) {
            std::cout << "准备存储日志到MySQL: " << std::endl;
            std::cout << "  ID: " << id << std::endl;
            std::cout << "  时间戳: " << timestamp << std::endl;
            std::cout << "  级别: " << level << std::endl;
            std::cout << "  源: " << source << std::endl;
            std::cout << "  消息: " << (message.length() > 30 ? message.substr(0, 27) + "..." : message) << std::endl;
        }
        
        // 准备MySQL日志条目
        storage::MySQLStorage::LogEntry entry;
        entry.id = id;
        entry.timestamp = timestamp;
        entry.level = level;
        entry.source = source;
        entry.message = message;
        
        // 准备字段和元数据，限制字段数量和大小
        int fieldCount = 0;
        const int MAX_FIELDS = 20;  // 限制字段数量
        
        for (const auto& [key, value] : record.fields) {
            if (fieldCount >= MAX_FIELDS) break;
            
            // 限制键和值的长度
            std::string k = (key.length() > 64) ? key.substr(0, 61) + "..." : key;
            std::string v = (value.length() > 255) ? value.substr(0, 252) + "..." : value;
            
            entry.fields[k] = v;
            fieldCount++;
        }
        
        // 尝试保存日志条目
        bool success = false;
        for (int attempt = 0; attempt < 3 && !success; attempt++) {
            try {
                success = mysqlStorage_->SaveLogEntry(entry);
                
                if (success && config_.debug) {
                    std::cout << "MySQL存储成功: 日志ID = " << id << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "MySQL存储尝试 " << (attempt + 1) << " 失败: " << e.what() << std::endl;
                std::this_thread::sleep_for(std::chrono::milliseconds(200));
            }
        }
        
        if (!success && config_.debug) {
            std::cerr << "所有MySQL存储尝试都失败: 日志ID = " << id << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "MySQL存储日志时出错: " << e.what() << std::endl;
    }
}

// 直接处理JSON字符串
bool LogProcessor::ProcessJsonString(const std::string& jsonStr) {
    if (jsonStr.empty()) {
        std::cerr << "ProcessJsonString: 空JSON字符串" << std::endl;
        return false;
    }
    
    try {
        std::cout << "直接处理JSON字符串: " << jsonStr.substr(0, 100) << (jsonStr.length() > 100 ? "..." : "") << std::endl;
        
        // 解析JSON
        nlohmann::json json = nlohmann::json::parse(jsonStr);
        
        // 创建日志数据
        LogData logData;
        logData.timestamp = std::chrono::system_clock::now();
        logData.message = jsonStr;
        logData.id = GenerateUUID();
        logData.source = "direct-json";
        logData.metadata["is_json"] = "true";
        
        // 提取基本字段
        if (json.contains("timestamp") && json["timestamp"].is_string()) {
            logData.metadata["timestamp"] = json["timestamp"].get<std::string>();
        }
        
        if (json.contains("level") && json["level"].is_string()) {
            logData.metadata["level"] = json["level"].get<std::string>();
        }
        
        if (json.contains("source") && json["source"].is_string()) {
            logData.source = json["source"].get<std::string>();
        }
        
        if (json.contains("message") && json["message"].is_string()) {
            logData.metadata["message"] = json["message"].get<std::string>();
        }
        
        if (json.contains("type") && json["type"].is_string()) {
            logData.metadata["type"] = json["type"].get<std::string>();
        }
        
        // 直接保存到MySQL
        if (mysqlStorage_) {
            try {
                std::cout << "直接保存JSON到MySQL: ID=" << logData.id << std::endl;
                
                storage::MySQLStorage::LogEntry entry;
                entry.id = logData.id;
                entry.timestamp = json.contains("timestamp") && json["timestamp"].is_string() 
                    ? json["timestamp"].get<std::string>() 
                    : TimestampToString(logData.timestamp);
                entry.level = json.contains("level") && json["level"].is_string() 
                    ? json["level"].get<std::string>() 
                    : "INFO";
                entry.source = json.contains("source") && json["source"].is_string() 
                    ? json["source"].get<std::string>() 
                    : "json-direct";
                entry.message = json.contains("message") && json["message"].is_string() 
                    ? json["message"].get<std::string>() 
                    : jsonStr;
                
                bool success = mysqlStorage_->SaveLogEntry(entry);
                if (success) {
                    std::cout << "成功保存JSON日志到MySQL: ID=" << entry.id << std::endl;
                    return true;
                } else {
                    std::cerr << "保存JSON日志到MySQL失败" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "保存JSON日志到MySQL异常: " << e.what() << std::endl;
            }
        }
        
        // 如果直接保存失败，尝试通过正常流程处理
        return ProcessLogData(std::move(logData)), true;
    } catch (const nlohmann::json::exception& e) {
        std::cerr << "解析JSON字符串失败: " << e.what() << std::endl;
        return false;
    } catch (const std::exception& e) {
        std::cerr << "处理JSON字符串异常: " << e.what() << std::endl;
        return false;
    }
}

} // namespace processor
} // namespace xumj 