#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <memory>
#include <chrono>
#include <random>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "../../include/xumj/processor/json_log_parser.h"
#include "../../include/xumj/common/log_entry.h"

// 生成随机UUID
std::string GenerateUUID() {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, 15);
    static std::uniform_int_distribution<> dis2(8, 11);

    std::stringstream ss;
    int i;
    ss << std::hex;
    for (i = 0; i < 8; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 4; i++) {
        ss << dis(gen);
    }
    ss << "-4";
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    ss << dis2(gen);
    for (i = 0; i < 3; i++) {
        ss << dis(gen);
    }
    ss << "-";
    for (i = 0; i < 12; i++) {
        ss << dis(gen);
    };
    return ss.str();
}

// 生成当前时间戳
std::string GetCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto in_time_t = std::chrono::system_clock::to_time_t(now);

    std::stringstream ss;
    ss << std::put_time(std::localtime(&in_time_t), "%Y-%m-%dT%H:%M:%S");
    return ss.str();
}

// 生成测试日志样本
std::vector<std::string> GenerateTestLogs(int count) {
    std::vector<std::string> logs;
    
    // 日志级别
    std::vector<std::string> levels = {"INFO", "WARNING", "ERROR", "DEBUG"};
    
    // 日志来源
    std::vector<std::string> sources = {
        "auth_service", "payment_service", "cart_service", 
        "database", "resource_monitor", "api_gateway"
    };
    
    // 日志消息模板
    std::vector<std::string> messages = {
        "用户登录成功: {user}",
        "用户登录失败: {user}, 原因: 密码错误",
        "支付处理超时: {transaction}",
        "购物车添加商品: {product}, 数量: {quantity}",
        "数据库查询耗时: {time}ms",
        "CPU使用率: {cpu}%",
        "内存使用: {memory}MB",
        "API请求: {endpoint}, 状态码: {status}"
    };
    
    // 随机生成器
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> level_dist(0, levels.size() - 1);
    std::uniform_int_distribution<> source_dist(0, sources.size() - 1);
    std::uniform_int_distribution<> message_dist(0, messages.size() - 1);
    std::uniform_int_distribution<> format_dist(0, 5); // 用于生成不同格式的日志
    
    for (int i = 0; i < count; ++i) {
        std::string id = GenerateUUID();
        std::string timestamp = GetCurrentTimestamp();
        std::string level = levels[level_dist(gen)];
        std::string source = sources[source_dist(gen)];
        std::string message = messages[message_dist(gen)];
        
        // 替换消息中的占位符
        if (message.find("{user}") != std::string::npos) {
            message.replace(message.find("{user}"), 6, "user" + std::to_string(i % 100));
        }
        if (message.find("{transaction}") != std::string::npos) {
            message.replace(message.find("{transaction}"), 13, "txn-" + std::to_string(i));
        }
        if (message.find("{product}") != std::string::npos) {
            message.replace(message.find("{product}"), 9, "product-" + std::to_string(i % 50));
        }
        if (message.find("{quantity}") != std::string::npos) {
            message.replace(message.find("{quantity}"), 10, std::to_string(1 + (i % 10)));
        }
        if (message.find("{time}") != std::string::npos) {
            message.replace(message.find("{time}"), 6, std::to_string(50 + (i % 500)));
        }
        if (message.find("{cpu}") != std::string::npos) {
            message.replace(message.find("{cpu}"), 5, std::to_string(10 + (i % 90)));
        }
        if (message.find("{memory}") != std::string::npos) {
            message.replace(message.find("{memory}"), 8, std::to_string(100 + (i % 900)));
        }
        if (message.find("{endpoint}") != std::string::npos) {
            message.replace(message.find("{endpoint}"), 10, "/api/v1/resource/" + std::to_string(i % 20));
        }
        if (message.find("{status}") != std::string::npos) {
            int status_codes[] = {200, 201, 400, 401, 403, 404, 500};
            message.replace(message.find("{status}"), 8, std::to_string(status_codes[i % 7]));
        }
        
        // 生成不同格式的JSON日志
        int format = format_dist(gen);
        std::string log;
        
        switch (format) {
            case 0: // 标准格式
                log = "{\"id\":\"" + id + "\",\"timestamp\":\"" + timestamp + 
                      "\",\"level\":\"" + level + "\",\"source\":\"" + source + 
                      "\",\"message\":\"" + message + "\"}";
                break;
                
            case 1: // 使用log_id而不是id
                log = "{\"log_id\":\"" + id + "\",\"timestamp\":\"" + timestamp + 
                      "\",\"level\":\"" + level + "\",\"source\":\"" + source + 
                      "\",\"message\":\"" + message + "\"}";
                break;
                
            case 2: // 使用time而不是timestamp
                log = "{\"id\":\"" + id + "\",\"time\":\"" + timestamp + 
                      "\",\"level\":\"" + level + "\",\"source\":\"" + source + 
                      "\",\"message\":\"" + message + "\"}";
                break;
                
            case 3: // 使用severity而不是level
                log = "{\"id\":\"" + id + "\",\"timestamp\":\"" + timestamp + 
                      "\",\"severity\":\"" + level + "\",\"source\":\"" + source + 
                      "\",\"message\":\"" + message + "\"}";
                break;
                
            case 4: // 使用service而不是source
                log = "{\"id\":\"" + id + "\",\"timestamp\":\"" + timestamp + 
                      "\",\"level\":\"" + level + "\",\"service\":\"" + source + 
                      "\",\"message\":\"" + message + "\"}";
                break;
                
            case 5: // 使用msg而不是message
                log = "{\"id\":\"" + id + "\",\"timestamp\":\"" + timestamp + 
                      "\",\"level\":\"" + level + "\",\"source\":\"" + source + 
                      "\",\"msg\":\"" + message + "\"}";
                break;
        }
        
        logs.push_back(log);
    }
    
    // 添加一些格式错误的日志
    if (count >= 100) {
        int errorCount = count * 0.1; // 10%的错误日志
        for (int i = 0; i < errorCount; ++i) {
            int index = i * 10; // 每10条放一个错误日志
            if (index < logs.size()) {
                // 生成各种错误类型
                switch (i % 5) {
                    case 0: // 缺少字段
                        logs[index] = "{\"id\":\"" + GenerateUUID() + "\",\"level\":\"ERROR\"}";
                        break;
                    case 1: // JSON格式错误
                        logs[index] = "{\"id\":\"" + GenerateUUID() + "\",\"timestamp\":\"" + 
                                     GetCurrentTimestamp() + "\"level\":\"ERROR\"}"; // 缺少逗号
                        break;
                    case 2: // 空JSON
                        logs[index] = "{}";
                        break;
                    case 3: // 非JSON字符串
                        logs[index] = "这不是一个JSON字符串";
                        break;
                    case 4: // 类型错误
                        logs[index] = "{\"id\":" + std::to_string(i) + ",\"timestamp\":\"" + 
                                     GetCurrentTimestamp() + "\",\"level\":123}"; // level应该是字符串
                        break;
                }
            }
        }
    }
    
    return logs;
}

// 简化版的JsonLogParser实现，用于测试
class SimpleJsonLogParser {
public:
    std::shared_ptr<xumj::LogEntry> Parse(const std::string& jsonString) {
        try {
            // 这里假设jsonString是一个有效的JSON，并且包含所有必需字段
            // 实际上，这是一个简化的实现，不包含错误处理
            
            // 解析JSON字符串
            rapidjson::Document doc;
            doc.Parse(jsonString.c_str());
            
            auto logEntry = std::make_shared<xumj::LogEntry>();
            
            // 假设所有字段都存在且格式正确
            logEntry->SetId(doc["id"].GetString());
            logEntry->SetTimestamp(doc["timestamp"].GetString());
            logEntry->SetLevel(doc["level"].GetString());
            logEntry->SetSource(doc["source"].GetString());
            logEntry->SetMessage(doc["message"].GetString());
            
            return logEntry;
        } catch (...) {
            // 任何异常都返回nullptr
            return nullptr;
        }
    }
};

// 增强版的JsonLogParser实现，用于测试
class EnhancedJsonLogParser {
public:
    std::shared_ptr<xumj::LogEntry> Parse(const std::string& jsonString) {
        try {
            // 解析JSON字符串
            rapidjson::Document doc;
            doc.Parse(jsonString.c_str());
            
            if (doc.HasParseError()) {
                std::cerr << "JSON parse error" << std::endl;
                return nullptr;
            }
            
            // 创建日志条目对象
            auto logEntry = std::make_shared<xumj::LogEntry>();
            
            // 处理ID字段 - 支持多种格式或生成默认值
            if (doc.HasMember("id") && doc["id"].IsString()) {
                logEntry->SetId(doc["id"].GetString());
            } else if (doc.HasMember("log_id") && doc["log_id"].IsString()) {
                logEntry->SetId(doc["log_id"].GetString());
            } else {
                // 生成UUID作为默认ID
                logEntry->SetId(GenerateUUID());
            }
            
            // 处理时间戳 - 支持多种格式
            if (doc.HasMember("timestamp") && doc["timestamp"].IsString()) {
                logEntry->SetTimestamp(doc["timestamp"].GetString());
            } else if (doc.HasMember("time") && doc["time"].IsString()) {
                logEntry->SetTimestamp(doc["time"].GetString());
            } else if (doc.HasMember("@timestamp") && doc["@timestamp"].IsString()) {
                logEntry->SetTimestamp(doc["@timestamp"].GetString());
            } else {
                // 使用当前时间作为默认值
                logEntry->SetTimestamp(GetCurrentTimestamp());
            }
            
            // 处理日志级别
            if (doc.HasMember("level") && doc["level"].IsString()) {
                logEntry->SetLevel(doc["level"].GetString());
            } else if (doc.HasMember("severity") && doc["severity"].IsString()) {
                logEntry->SetLevel(doc["severity"].GetString());
            } else {
                logEntry->SetLevel("INFO");  // 默认级别
            }
            
            // 处理来源
            if (doc.HasMember("source") && doc["source"].IsString()) {
                logEntry->SetSource(doc["source"].GetString());
            } else if (doc.HasMember("service") && doc["service"].IsString()) {
                logEntry->SetSource(doc["service"].GetString());
            } else {
                logEntry->SetSource("unknown");
            }
            
            // 处理消息内容
            if (doc.HasMember("message") && doc["message"].IsString()) {
                logEntry->SetMessage(doc["message"].GetString());
            } else if (doc.HasMember("msg") && doc["msg"].IsString()) {
                logEntry->SetMessage(doc["msg"].GetString());
            } else {
                // 如果没有消息字段，将整个JSON作为消息
                rapidjson::StringBuffer buffer;
                rapidjson::Writer<rapidjson::StringBuffer> writer(buffer);
                doc.Accept(writer);
                logEntry->SetMessage(buffer.GetString());
            }
            
            return logEntry;
        } catch (const std::exception& e) {
            std::cerr << "Exception in JSON parsing: " << e.what() << std::endl;
            return nullptr;
        }
    }
};

// 测量解析器成功率
template <typename ParserType>
void MeasureParserSuccessRate(const std::vector<std::string>& testLogs, const std::string& parserName) {
    int totalLogs = testLogs.size();
    int successfulParses = 0;
    
    // 创建解析器实例
    ParserType parser;
    
    // 记录解析结果
    for (const auto& log : testLogs) {
        try {
            auto result = parser.Parse(log);
            if (result != nullptr) {
                successfulParses++;
            }
        } catch (...) {
            // 解析失败，不增加成功计数
        }
    }
    
    // 计算成功率
    double successRate = (double)successfulParses / totalLogs * 100.0;
    std::cout << parserName << " 解析成功率: " << successRate << "% (" 
              << successfulParses << "/" << totalLogs << ")" << std::endl;
}

int main() {
    // 生成1000条测试日志
    std::cout << "生成测试日志样本..." << std::endl;
    std::vector<std::string> testLogs = GenerateTestLogs(1000);
    
    std::cout << "测试日志样本生成完成，共 " << testLogs.size() << " 条" << std::endl;
    
    // 测试简化版解析器
    std::cout << "\n测试简化版解析器（无健壮性处理）..." << std::endl;
    MeasureParserSuccessRate<SimpleJsonLogParser>(testLogs, "简化版解析器");
    
    // 测试增强版解析器
    std::cout << "\n测试增强版解析器（有健壮性处理）..." << std::endl;
    MeasureParserSuccessRate<EnhancedJsonLogParser>(testLogs, "增强版解析器");
    
    return 0;
} 