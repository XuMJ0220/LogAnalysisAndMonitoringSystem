#include "xumj/processor/log_processor.h"
#include "xumj/storage/storage_factory.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <nlohmann/json.hpp>
#include <zlib.h>
#include <uuid/uuid.h>

namespace xumj {
namespace processor {

// 辅助函数：生成UUID
std::string GenerateUUID() {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    return std::string(uuid_str);
}

// 辅助函数：解压缩数据
std::string DecompressData(const std::string& compressedData) {
    if (compressedData.empty()) {
        return "";
    }
    
    z_stream zs;
    memset(&zs, 0, sizeof(zs));
    
    if (inflateInit(&zs) != Z_OK) {
        throw std::runtime_error("解压缩初始化失败");
    }
    
    zs.next_in = (Bytef*)compressedData.data();
    zs.avail_in = compressedData.size();
    
    int ret;
    char outbuffer[32768];
    std::string outstring;
    
    do {
        zs.next_out = reinterpret_cast<Bytef*>(outbuffer);
        zs.avail_out = sizeof(outbuffer);
        
        ret = inflate(&zs, Z_NO_FLUSH);
        
        if (outstring.size() < zs.total_out) {
            outstring.append(outbuffer, zs.total_out - outstring.size());
        }
    } while (ret == Z_OK);
    
    inflateEnd(&zs);
    
    if (ret != Z_STREAM_END) {
        throw std::runtime_error("解压缩数据出错");
    }
    
    return outstring;
}

// 辅助函数：时间戳转字符串
std::string TimestampToString(const std::chrono::system_clock::time_point& timestamp) {
    auto time_t_now = std::chrono::system_clock::to_time_t(timestamp);
    std::tm tm_now = *std::localtime(&time_t_now);
    char buffer[30];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_now);
    return std::string(buffer);
}

// 压缩字符串函数
std::string CompressString(const std::string& str) {
    if (str.empty()) {
        return {};
    }
    
    // 计算压缩后可能的最大大小
    uLongf compressedSize = compressBound(static_cast<uLong>(str.size()));
    std::vector<Bytef> compressedData(compressedSize);
    
    // 压缩数据
    int result = compress(
        compressedData.data(),
        &compressedSize,
        reinterpret_cast<const Bytef*>(str.data()),
        static_cast<uLong>(str.size())
    );
    
    if (result != Z_OK) {
        // 压缩失败
        return {};
    }
    
    // 返回压缩后的数据
    return std::string(
        reinterpret_cast<char*>(compressedData.data()),
        compressedSize
    );
}

// 解压字符串函数
std::string DecompressString(const std::string& compressedStr) {
    if (compressedStr.empty()) {
        return {};
    }
    
    // 假设解压后的数据是压缩前的10倍（这是一个估计值）
    uLongf decompressedSize = static_cast<uLongf>(compressedStr.size() * 10);
    std::vector<Bytef> decompressedData(decompressedSize);
    
    // 尝试解压
    int result = uncompress(
        decompressedData.data(),
        &decompressedSize,
        reinterpret_cast<const Bytef*>(compressedStr.data()),
        static_cast<uLong>(compressedStr.size())
    );
    
    if (result != Z_OK) {
        // 如果第一次尝试失败，可能是因为缓冲区太小
        // 增加缓冲区大小再试一次
        decompressedSize *= 10;
        decompressedData.resize(decompressedSize);
        
        result = uncompress(
            decompressedData.data(),
            &decompressedSize,
            reinterpret_cast<const Bytef*>(compressedStr.data()),
            static_cast<uLong>(compressedStr.size())
        );
        
        if (result != Z_OK) {
            // 解压失败
            return {};
        }
    }
    
    // 返回解压后的数据
    return std::string(
        reinterpret_cast<char*>(decompressedData.data()),
        decompressedSize
    );
}

// RegexLogParser实现

RegexLogParser::RegexLogParser(const std::string& name,
                             const std::string& pattern,
                             const std::unordered_map<int, std::string>& fieldMapping)
    : name_(name), pattern_(pattern), fieldMapping_(fieldMapping) {
}

analyzer::LogRecord RegexLogParser::Parse(const LogData& data) const {
    analyzer::LogRecord record;
    
    // 设置基本字段
    record.id = data.id;
    record.timestamp = TimestampToString(data.timestamp);
    record.source = data.source;
    
    // 获取日志内容（解压如果需要）
    std::string content = data.compressed ? DecompressData(data.data) : data.data;
    
    try {
        // 创建正则表达式
        std::regex regexPattern(pattern_);
        std::smatch matches;
        
        // 匹配日志内容
        if (std::regex_search(content, matches, regexPattern)) {
            // 提取字段
            for (const auto& [groupIndex, fieldName] : fieldMapping_) {
                if (groupIndex > 0 && groupIndex < static_cast<int>(matches.size())) {
                    if (fieldName == "level") {
                        record.level = matches[groupIndex].str();
                    } else if (fieldName == "message") {
                        record.message = matches[groupIndex].str();
                    } else {
                        record.fields[fieldName] = matches[groupIndex].str();
                    }
                }
            }
            
            // 确保基本字段存在
            if (record.level.empty()) {
                record.level = "INFO";  // 默认级别
            }
            
            if (record.message.empty() && matches.size() > 0) {
                record.message = matches[0].str();  // 使用整个匹配作为消息
            }
        } else {
            // 如果不匹配，使用原始内容作为消息
            record.level = "INFO";
            record.message = content;
        }
    } catch (const std::regex_error& e) {
        // 处理正则表达式错误
        record.level = "ERROR";
        record.message = "解析错误: " + std::string(e.what()) + " - " + content;
    }
    
    // 添加元数据作为字段
    for (const auto& [key, value] : data.metadata) {
        record.fields["metadata." + key] = value;
    }
    
    return record;
}

std::string RegexLogParser::GetName() const {
    return name_;
}

bool RegexLogParser::CanParse(const LogData& data) const {
    // 检查数据是否可以被此解析器处理
    if (data.compressed) {
        return false;  // 无法预先检查压缩数据，需要先解压
    }
    
    try {
        std::regex regexPattern(pattern_);
        return std::regex_search(data.data, regexPattern);
    } catch (...) {
        return false;
    }
}

// JsonLogParser实现

JsonLogParser::JsonLogParser(const std::string& name,
                           const std::unordered_map<std::string, std::string>& fieldMapping)
    : name_(name), fieldMapping_(fieldMapping) {
}

analyzer::LogRecord JsonLogParser::Parse(const LogData& data) const {
    using json = nlohmann::json;
    analyzer::LogRecord record;
    
    // 设置基本字段
    record.id = data.id;
    record.timestamp = TimestampToString(data.timestamp);
    record.source = data.source;
    
    // 获取日志内容（解压如果需要）
    std::string content = data.compressed ? DecompressData(data.data) : data.data;
    
    try {
        // 解析JSON
        json j = json::parse(content);
        
        // 映射字段
        for (const auto& [jsonField, recordField] : fieldMapping_) {
            if (j.contains(jsonField)) {
                if (recordField == "level") {
                    record.level = j[jsonField].get<std::string>();
                } else if (recordField == "message") {
                    record.message = j[jsonField].get<std::string>();
                } else {
                    record.fields[recordField] = j[jsonField].dump();
                }
            }
        }
        
        // 确保基本字段存在
        if (record.level.empty()) {
            record.level = "INFO";  // 默认级别
        }
        
        if (record.message.empty()) {
            record.message = content;  // 使用整个JSON作为消息
        }
        
        // 处理额外的JSON字段
        for (auto it = j.begin(); it != j.end(); ++it) {
            bool found = false;
            for (const auto& [jsonField, _] : fieldMapping_) {
                if (it.key() == jsonField) {
                    found = true;
                    break;
                }
            }
            
            if (!found) {
                record.fields["json." + it.key()] = it.value().dump();
            }
        }
    } catch (const std::exception& e) {
        // 处理JSON解析错误
        record.level = "ERROR";
        record.message = "JSON解析错误: " + std::string(e.what()) + " - " + content;
    }
    
    // 添加元数据作为字段
    for (const auto& [key, value] : data.metadata) {
        record.fields["metadata." + key] = value;
    }
    
    return record;
}

std::string JsonLogParser::GetName() const {
    return name_;
}

bool JsonLogParser::CanParse(const LogData& data) const {
    try {
        // 尝试解析JSON，如果成功则返回true
        [[maybe_unused]] auto json = nlohmann::json::parse(data.data);
        return true;
    } catch (const nlohmann::json::exception&) {
        // 解析失败，不是有效的JSON
        return false;
    }
}

// LogProcessor实现

LogProcessor::LogProcessor(const ProcessorConfig& config)
    : config_(config),
      dataCount_(0),
      analyzer_(std::make_shared<analyzer::LogAnalyzer>(config.analyzerConfig)),
      tcpServer_(nullptr),
      threadPool_(std::make_unique<common::ThreadPool>(config.threadPoolSize)),
      processThread_(),
      running_(false) {
    
    // 初始化存储
    if (!config_.redisConfigJson.empty()) {
        try {
            auto redisConfig = storage::StorageFactory::CreateRedisConfigFromJson(config_.redisConfigJson);
            redisStorage_ = std::make_shared<storage::RedisStorage>(redisConfig);
            std::cout << "Redis存储初始化成功" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "Redis存储初始化失败: " << e.what() << std::endl;
        }
    }
    
    if (!config_.mysqlConfigJson.empty()) {
        try {
            auto mysqlConfig = storage::StorageFactory::CreateMySQLConfigFromJson(config_.mysqlConfigJson);
            mysqlStorage_ = std::make_shared<storage::MySQLStorage>(mysqlConfig);
            mysqlStorage_->Initialize();
            std::cout << "MySQL存储初始化成功" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "MySQL存储初始化失败: " << e.what() << std::endl;
        }
    }
    
    // 注册默认解析器
    AddParser(std::make_shared<JsonLogParser>("JsonParser", 
        std::unordered_map<std::string, std::string>{
            {"message", "message"},
            {"level", "level"},
            {"timestamp", "timestamp"},
            {"source", "source"}
        }
    ));
    
    AddParser(std::make_shared<RegexLogParser>("SimpleLogParser",
        R"(\[(.*?)\]\s+\[(.*?)\]\s+\[(.*?)\]:\s+(.*))",
        std::unordered_map<int, std::string>{
            {1, "timestamp"},
            {2, "level"},
            {3, "source"},
            {4, "message"}
        }
    ));
}

LogProcessor::~LogProcessor() {
    Stop();
}

bool LogProcessor::Initialize(const ProcessorConfig&) {
    // 配置已经在构造函数中初始化
    return true;
}

void LogProcessor::AddParser(std::shared_ptr<LogParser> parser) {
    if (parser) {
        std::lock_guard<std::mutex> lock(parsersMutex_);
        parsers_.push_back(std::move(parser));
    }
}

void LogProcessor::ClearParsers() {
    std::lock_guard<std::mutex> lock(parsersMutex_);
    parsers_.clear();
}

bool LogProcessor::SubmitLogData(const LogData& data) {
    if (!running_) {
        return false;
    }
    
    // 检查队列大小
    if (GetPendingCount() >= config_.maxQueueSize) {
        return false;  // 队列已满
    }
    
    // 添加数据到待处理队列
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        pendingData_.push(data);
        dataCount_++;
    }
    
    return true;
}

size_t LogProcessor::SubmitLogDataBatch(const std::vector<LogData>& dataList) {
    if (!running_) {
        return 0;
    }
    
    size_t count = 0;
    
    // 添加数据到待处理队列
    {
        std::lock_guard<std::mutex> lock(dataMutex_);
        
        for (const auto& data : dataList) {
            // 检查队列大小
            if (dataCount_ >= config_.maxQueueSize) {
                break;  // 队列已满
            }
            
            pendingData_.push(data);
            dataCount_++;
            count++;
        }
    }
    
    return count;
}

void LogProcessor::SetProcessCallback(ProcessCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    processCallback_ = std::move(callback);
}

bool LogProcessor::Start() {
    if (running_) {
        return true;  // 已经在运行
    }
    
    // 启动分析器
    if (analyzer_) {
        analyzer_->Start();
    }
    
    // 启动处理线程
    running_ = true;
    processThread_ = std::thread(&LogProcessor::ProcessThreadFunc, this);
    
    return true;
}

void LogProcessor::Stop() {
    if (!running_) {
        return;  // 已经停止
    }
    
    // 停止处理线程
    running_ = false;
    
    // 等待线程结束
    if (processThread_.joinable()) {
        processThread_.join();
    }
    
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
        std::lock_guard<std::mutex> lock(dataMutex_);
        while (!pendingData_.empty()) {
            pendingData_.pop();
        }
        dataCount_ = 0;
    }
}

bool LogProcessor::IsRunning() const {
    return running_;
}

size_t LogProcessor::GetParserCount() const {
    std::lock_guard<std::mutex> lock(parsersMutex_);
    return parsers_.size();
}

size_t LogProcessor::GetPendingCount() const {
    std::lock_guard<std::mutex> lock(dataMutex_);
    return dataCount_;
}

std::shared_ptr<analyzer::LogAnalyzer> LogProcessor::GetAnalyzer() {
    return analyzer_;
}

void LogProcessor::ProcessThreadFunc() {
    while (running_) {
        std::vector<LogData> batch;
        
        // 获取一批待处理数据
        {
            std::lock_guard<std::mutex> lock(dataMutex_);
            
            size_t count = std::min(config_.batchSize, dataCount_);
            for (size_t i = 0; i < count && !pendingData_.empty(); ++i) {
                batch.push_back(pendingData_.front());
                pendingData_.pop();
            }
            dataCount_ -= batch.size();
        }
        
        // 处理当前批次
        for (const auto& data : batch) {
            // 在线程池中异步处理每条数据
            threadPool_->Submit([this, data]() {
                this->ProcessLogData(data);
            });
        }
        
        // 等待指定的处理间隔
        if (batch.empty()) {
            std::this_thread::sleep_for(config_.processInterval);
        }
    }
}

void LogProcessor::ProcessLogData(const LogData& data) {
    try {
        // 解析日志数据
        analyzer::LogRecord record = ParseLogData(data);
        
        // 存档原始日志数据
        ArchiveLogData(data, record);
        
        // 将记录提交给分析器
        if (analyzer_) {
            analyzer_->SubmitRecord(record);
        }
        
        // 调用成功回调
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (processCallback_) {
                processCallback_(data.id, true);
            }
        }
    } catch (const std::exception& e) {
        std::cerr << "处理日志数据失败: " << e.what() << std::endl;
        
        // 调用失败回调
        {
            std::lock_guard<std::mutex> lock(callbackMutex_);
            if (processCallback_) {
                processCallback_(data.id, false);
            }
        }
    }
}

analyzer::LogRecord LogProcessor::ParseLogData(const LogData& data) {
    std::vector<std::shared_ptr<LogParser>> currentParsers;
    
    // 获取当前解析器集合
    {
        std::lock_guard<std::mutex> lock(parsersMutex_);
        currentParsers = parsers_;
    }
    
    // 首先尝试找到一个可以解析的解析器
    for (const auto& parser : currentParsers) {
        if (parser->CanParse(data)) {
            return parser->Parse(data);
        }
    }
    
    // 如果没有找到匹配的解析器，使用第一个解析器，或创建一个简单的记录
    if (!currentParsers.empty()) {
        return currentParsers[0]->Parse(data);
    } else {
        // 创建一个基本的日志记录
        analyzer::LogRecord record;
        record.id = data.id;
        record.timestamp = TimestampToString(data.timestamp);
        record.level = "INFO";
        record.source = data.source;
        record.message = data.compressed ? DecompressData(data.data) : data.data;
        
        // 添加元数据
        for (const auto& [key, value] : data.metadata) {
            record.fields["metadata." + key] = value;
        }
        
        return record;
    }
}

void LogProcessor::ArchiveLogData(const LogData& data, const analyzer::LogRecord& record) {
    // 存储原始日志数据（可选）
    if (redisStorage_) {
        try {
            // 使用Redis存储原始日志
            std::string key = "raw_log:" + data.id;
            
            // 存储原始数据
            if (config_.compressArchive && !data.compressed) {
                // 如果需要压缩且数据未压缩，则压缩后存储
                std::string compressedData = CompressString(data.data);
                redisStorage_->Set(key, compressedData);
                redisStorage_->HashSet(key + ":info", "compressed", "true");
            } else {
                // 否则直接存储
                redisStorage_->Set(key, data.data);
                redisStorage_->HashSet(key + ":info", "compressed", data.compressed ? "true" : "false");
            }
            
            // 存储元数据
            redisStorage_->HashSet(key + ":info", "timestamp", TimestampToString(data.timestamp));
            redisStorage_->HashSet(key + ":info", "source", data.source);
            
            for (const auto& [metaKey, metaValue] : data.metadata) {
                redisStorage_->HashSet(key + ":info", metaKey, metaValue);
            }
            
            // 设置过期时间（可根据需要调整）
            redisStorage_->Expire(key, 86400 * 7);  // 7天
            redisStorage_->Expire(key + ":info", 86400 * 7);  // 7天
            
        } catch (const storage::RedisStorageException& e) {
            std::cerr << "存储原始日志到Redis失败: " << e.what() << std::endl;
        }
    }
    
    // 将解析后的日志记录存储到MySQL（可选）
    if (mysqlStorage_) {
        try {
            // 创建MySQL日志条目
            storage::MySQLStorage::LogEntry entry;
            entry.id = record.id;
            entry.timestamp = record.timestamp;
            entry.level = record.level;
            entry.source = record.source;
            entry.message = record.message;
            
            // 复制字段
            for (const auto& [fieldKey, fieldValue] : record.fields) {
                entry.fields[fieldKey] = fieldValue;
            }
            
            // 保存到MySQL
            mysqlStorage_->SaveLogEntry(entry);
            
        } catch (const storage::MySQLStorageException& e) {
            std::cerr << "存储日志到MySQL失败: " << e.what() << std::endl;
        }
    }
}

std::string LogProcessor::DecompressLogData(const LogData& data) {
    if (!data.compressed) {
        return data.data;  // 未压缩，直接返回
    }
    
    try {
        return DecompressData(data.data);
    } catch (const std::exception& e) {
        std::cerr << "解压缩日志数据失败: " << e.what() << std::endl;
        return data.data;  // 失败时返回原始数据
    }
}

bool LogProcessor::InitializeTcpServer() {
    try {
        // 创建TCP服务器，监听特定端口
        tcpServer_ = std::make_unique<network::TcpServer>("LogServer", "0.0.0.0", 9000, 4);
        
        // 设置消息回调
        tcpServer_->SetMessageCallback(std::bind(&LogProcessor::HandleTcpMessage, this, 
                                              std::placeholders::_1, 
                                              std::placeholders::_2,
                                              std::placeholders::_3));
        
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

void LogProcessor::HandleTcpMessage(uint64_t connectionId, const std::string& message, muduo::Timestamp /* timestamp */) {
    // 创建日志数据
    LogData data;
    data.id = "tcp-" + std::to_string(connectionId) + "-" + std::to_string(dataCount_++);
    data.data = message;
    data.timestamp = std::chrono::system_clock::now();
    
    // 尝试获取连接信息作为来源
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        auto it = connections_.find(connectionId);
        if (it != connections_.end()) {
            data.source = it->second;
        } else {
            data.source = "unknown";
        }
    }
    
    // 提交日志数据进行处理
    SubmitLogData(data);
}

void LogProcessor::HandleTcpConnection(uint64_t connectionId, const std::string& clientAddr, bool connected) {
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

} // namespace processor
} // namespace xumj 