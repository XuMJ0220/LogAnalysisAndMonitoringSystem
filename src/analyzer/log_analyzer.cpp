#include "xumj/analyzer/log_analyzer.h"
#include "xumj/storage/storage_factory.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <algorithm>

namespace xumj {
namespace analyzer {

// RegexAnalysisRule实现

RegexAnalysisRule::RegexAnalysisRule(const std::string& name,
                                  const std::string& pattern,
                                  const std::vector<std::string>& fieldNames)
    : name_(name), pattern_(pattern), fieldNames_(fieldNames) {
}

std::unordered_map<std::string, std::string> RegexAnalysisRule::Analyze(const LogRecord& record) const {
    std::unordered_map<std::string, std::string> results;
    
    try {
        // 创建正则表达式对象
        std::regex regexPattern(pattern_);
        
        // 匹配日志消息
        std::smatch matches;
        if (std::regex_search(record.message, matches, regexPattern)) {
            // 提取匹配组并映射到字段名
            for (size_t i = 1; i < matches.size() && i - 1 < fieldNames_.size(); ++i) {
                if (matches[i].matched) {
                    results[fieldNames_[i - 1]] = matches[i].str();
                }
            }
            
            // 添加匹配结果标志
            results["matched"] = "true";
            results["rule"] = name_;
            
            // 添加has_error字段，如果模式包含error/exception/failed相关词汇，则设为true
            if (pattern_.find("error") != std::string::npos || 
                pattern_.find("exception") != std::string::npos || 
                pattern_.find("failed") != std::string::npos) {
                results["has_error"] = "true";
            }
        } else {
            results["matched"] = "false";
            
            // 添加has_error字段，设为false
            if (fieldNames_.size() > 0 && fieldNames_[0] == "has_error") {
                results["has_error"] = "false";
            }
        }
    } catch (const std::regex_error& e) {
        // 处理正则表达式错误
        results["error"] = "正则表达式错误: " + std::string(e.what());
    }
    
    return results;
}

std::string RegexAnalysisRule::GetName() const {
    return name_;
}

// KeywordAnalysisRule实现

KeywordAnalysisRule::KeywordAnalysisRule(const std::string& name,
                                      const std::vector<std::string>& keywords,
                                      bool scoring)
    : name_(name), keywords_(keywords), scoring_(scoring) {
}

std::unordered_map<std::string, std::string> KeywordAnalysisRule::Analyze(const LogRecord& record) const {
    std::unordered_map<std::string, std::string> results;
    
    // 将日志消息转为小写以进行不区分大小写的匹配
    std::string lowerMessage = record.message;
    std::transform(lowerMessage.begin(), lowerMessage.end(), lowerMessage.begin(),
                  [](unsigned char c) { return std::tolower(c); });
    
    // 匹配关键字
    int matchCount = 0;
    std::vector<std::string> matchedKeywords;
    
    for (const auto& keyword : keywords_) {
        // 关键字转小写
        std::string lowerKeyword = keyword;
        std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        // 查找关键字
        if (lowerMessage.find(lowerKeyword) != std::string::npos) {
            matchCount++;
            matchedKeywords.push_back(keyword);
        }
    }
    
    // 生成结果
    if (matchCount > 0) {
        results["matched"] = "true";
        results["rule"] = name_;
        results["match_count"] = std::to_string(matchCount);
        
        // 如果进行打分，计算匹配度得分(0-100)
        if (scoring_ && !keywords_.empty()) {
            int score = (matchCount * 100) / static_cast<int>(keywords_.size());
            results["score"] = std::to_string(score);
        }
        
        // 存储匹配的关键字列表
        std::stringstream ss;
        for (size_t i = 0; i < matchedKeywords.size(); ++i) {
            if (i > 0) ss << ", ";
            ss << matchedKeywords[i];
        }
        results["matched_keywords"] = ss.str();
    } else {
        results["matched"] = "false";
    }
    
    return results;
}

std::string KeywordAnalysisRule::GetName() const {
    return name_;
}

// LogAnalyzer实现

LogAnalyzer::LogAnalyzer(const AnalyzerConfig& config) 
    : running_(false) {
    Initialize(config);
}

LogAnalyzer::~LogAnalyzer() {
    Stop();
}

bool LogAnalyzer::Initialize(const AnalyzerConfig& config) {
    // 如果分析器已在运行，先停止它
    if (running_) {
        Stop();
    }
    
    // 保存配置
    config_ = config;
    
    // 初始化线程池
    threadPool_ = std::make_unique<common::ThreadPool>(config_.threadPoolSize);
    
    // 初始化存储
    if (config_.storeResults) {
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
    }
    
    return true;
}

void LogAnalyzer::AddRule(std::shared_ptr<AnalysisRule> rule) {
    if (rule) {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        rules_.push_back(std::move(rule));
    }
}

void LogAnalyzer::ClearRules() {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    rules_.clear();
}

bool LogAnalyzer::SubmitRecord(const LogRecord& record) {
    if (!running_) {
        return false;
    }
    
    // 添加记录到待处理队列
    {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        pendingRecords_.push_back(record);
    }
    
    return true;
}

size_t LogAnalyzer::SubmitRecords(const std::vector<LogRecord>& records) {
    if (!running_) {
        return 0;
    }
    
    size_t count = 0;
    
    // 添加记录到待处理队列
    {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        for (const auto& record : records) {
            pendingRecords_.push_back(record);
            count++;
        }
    }
    
    return count;
}

void LogAnalyzer::SetAnalysisCallback(AnalysisCallback callback) {
    std::lock_guard<std::mutex> lock(callbackMutex_);
    analysisCallback_ = std::move(callback);
}

bool LogAnalyzer::Start() {
    if (running_) {
        return true;  // 已经在运行
    }
    
    // 启动分析线程
    running_ = true;
    analyzeThread_ = std::thread(&LogAnalyzer::AnalyzeThreadFunc, this);
    
    return true;
}

void LogAnalyzer::Stop() {
    if (!running_) {
        return;  // 已经停止
    }
    
    // 停止分析线程
    running_ = false;
    
    // 等待线程结束
    if (analyzeThread_.joinable()) {
        analyzeThread_.join();
    }
    
    // 清空待处理队列
    {
        std::lock_guard<std::mutex> lock(recordsMutex_);
        pendingRecords_.clear();
    }
}

bool LogAnalyzer::IsRunning() const {
    return running_;
}

size_t LogAnalyzer::GetRuleCount() const {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    return rules_.size();
}

size_t LogAnalyzer::GetPendingCount() const {
    std::lock_guard<std::mutex> lock(recordsMutex_);
    return pendingRecords_.size();
}

void LogAnalyzer::AnalyzeThreadFunc() {
    while (running_) {
        std::vector<LogRecord> batch;
        
        // 获取一批待处理记录
        {
            std::lock_guard<std::mutex> lock(recordsMutex_);
            
            // 从待处理队列中取出批次大小的记录
            size_t count = std::min(config_.batchSize, pendingRecords_.size());
            if (count > 0) {
                batch.insert(batch.end(), pendingRecords_.begin(), pendingRecords_.begin() + count);
                pendingRecords_.erase(pendingRecords_.begin(), pendingRecords_.begin() + count);
            }
        }
        
        // 处理当前批次
        for (const auto& record : batch) {
            // 在线程池中异步处理每条记录
            threadPool_->Submit([this, record]() {
                this->ProcessRecord(record);
            });
        }
        
        // 等待指定的分析间隔
        if (batch.empty()) {
            std::this_thread::sleep_for(config_.analyzeInterval);
        }
    }
}

void LogAnalyzer::ProcessRecord(const LogRecord& record) {
    std::vector<std::shared_ptr<AnalysisRule>> currentRules;
    
    // 获取当前规则集合（避免长时间持有锁）
    {
        std::lock_guard<std::mutex> lock(rulesMutex_);
        currentRules = rules_;
    }
    
    // 应用所有规则
    std::unordered_map<std::string, std::string> combinedResults;
    
    for (const auto& rule : currentRules) {
        // 应用规则
        auto results = rule->Analyze(record);
        
        // 如果有匹配结果，合并到总结果中
        if (results["matched"] == "true") {
            for (const auto& [key, value] : results) {
                // 规则名作为前缀避免冲突
                std::string prefixedKey = rule->GetName() + "." + key;
                combinedResults[prefixedKey] = value;
            }
        }
    }
    
    // 添加基本信息到结果中
    combinedResults["record.id"] = record.id;
    combinedResults["record.timestamp"] = record.timestamp;
    combinedResults["record.level"] = record.level;
    combinedResults["record.source"] = record.source;
    
    // 存储分析结果
    if (config_.storeResults) {
        // 存储到Redis
        if (redisStorage_) {
            StoreResultToRedis(record.id, combinedResults);
        }
        
        // 存储到MySQL
        if (mysqlStorage_) {
            StoreResultToMySQL(record.id, combinedResults);
        }
    }
    
    // 调用回调函数
    {
        std::lock_guard<std::mutex> lock(callbackMutex_);
        if (analysisCallback_) {
            analysisCallback_(record.id, combinedResults);
        }
    }
}

void LogAnalyzer::StoreResultToRedis(const std::string& recordId, 
                                   const std::unordered_map<std::string, std::string>& results) {
    try {
        // 使用散列表存储分析结果
        std::string key = "analysis_result:" + recordId;
        
        // 存储每个字段
        for (const auto& [field, value] : results) {
            redisStorage_->HashSet(key, field, value);
        }
        
        // 设置过期时间（可根据需要调整）
        redisStorage_->Expire(key, 86400);  // 24小时
        
        // 将ID添加到最近分析结果集
        redisStorage_->SetAdd("recent_analysis_results", recordId);
        
    } catch (const storage::RedisStorageException& e) {
        std::cerr << "存储分析结果到Redis失败: " << e.what() << std::endl;
    }
}

void LogAnalyzer::StoreResultToMySQL(const std::string& recordId,
                                   const std::unordered_map<std::string, std::string>& results) {
    try {
        // 创建分析结果日志条目
        storage::MySQLStorage::LogEntry entry;
        entry.id = recordId;
        
        // 从结果中提取基本字段
        if (results.count("record.timestamp")) {
            entry.timestamp = results.at("record.timestamp");
        } else {
            // 使用当前时间作为备用
            auto now = std::chrono::system_clock::now();
            auto time_t_now = std::chrono::system_clock::to_time_t(now);
            std::tm tm_now = *std::localtime(&time_t_now);
            char buffer[30];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_now);
            entry.timestamp = buffer;
        }
        
        if (results.count("record.level")) {
            entry.level = results.at("record.level");
        } else {
            entry.level = "INFO";
        }
        
        if (results.count("record.source")) {
            entry.source = results.at("record.source");
        } else {
            entry.source = "LogAnalyzer";
        }
        
        // 构建消息（可根据需要自定义格式）
        std::stringstream message;
        message << "分析结果: ";
        
        bool firstItem = true;
        for (const auto& [key, value] : results) {
            if (key.compare(0, 7, "record.") != 0) {  // 排除基本字段
                if (!firstItem) {
                    message << ", ";
                }
                message << key << "=" << value;
                firstItem = false;
            }
            
            // 将所有分析结果作为字段存储
            entry.fields[key] = value;
        }
        
        entry.message = message.str();
        
        // 保存到MySQL
        mysqlStorage_->SaveLogEntry(entry);
        
    } catch (const storage::MySQLStorageException& e) {
        std::cerr << "存储分析结果到MySQL失败: " << e.what() << std::endl;
    }
}

} // namespace analyzer
} // namespace xumj 