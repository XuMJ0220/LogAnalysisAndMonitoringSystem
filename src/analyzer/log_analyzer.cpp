#include "xumj/analyzer/log_analyzer.h"
#include "xumj/storage/storage_factory.h"
#include <iostream>
#include <sstream>
#include <regex>
#include <algorithm>
#include <chrono>

namespace xumj {
namespace analyzer {

// RegexAnalysisRule实现

RegexAnalysisRule::RegexAnalysisRule(const std::string& name,
                                  const std::string& pattern,
                                  const std::vector<std::string>& fieldNames)
    : name_(name), pattern_(pattern), fieldNames_(fieldNames) {
    // 预编译正则表达式
    try {
        regexPattern_ = std::make_unique<std::regex>(pattern_);
    } catch (const std::regex_error& e) {
        std::cerr << "正则表达式编译错误: " << e.what() << std::endl;
    }
}

std::string RegexAnalysisRule::GetName() const {
    return name_;
}

std::unordered_map<std::string, std::string> RegexAnalysisRule::Analyze(const LogRecord& record) const {
    std::unordered_map<std::string, std::string> results;
    
    if (!config_.enabled) {
        results["enabled"] = "false";
        return results;
    }
    
    try {
        if (!regexPattern_) {
            throw std::runtime_error("正则表达式未编译");
        }
        
        // 匹配日志消息
        std::smatch matches;
        if (std::regex_search(record.message, matches, *regexPattern_)) {
            // 提取匹配组并映射到字段名
            for (size_t i = 1; i < matches.size() && i - 1 < fieldNames_.size(); ++i) {
                if (matches[i].matched) {
                    results[fieldNames_[i - 1]] = matches[i].str();
                }
            }
            
            results["matched"] = "true";
            results["rule"] = name_;
            results["group"] = config_.group;
            
            if (pattern_.find("error") != std::string::npos || 
                pattern_.find("exception") != std::string::npos || 
                pattern_.find("failed") != std::string::npos) {
                results["has_error"] = "true";
            }
        } else {
            results["matched"] = "false";
            results["group"] = config_.group;
        }
    } catch (const std::exception& e) {
        results["error"] = "分析错误: " + std::string(e.what());
    }
    
    return results;
}

const RuleConfig& RegexAnalysisRule::GetConfig() const {
    return config_;
}

void RegexAnalysisRule::SetConfig(const RuleConfig& config) {
    config_ = config;
}

void RegexAnalysisRule::Enable() {
    config_.enabled = true;
}

void RegexAnalysisRule::Disable() {
    config_.enabled = false;
}

bool RegexAnalysisRule::IsEnabled() const {
    return config_.enabled;
}

// KeywordAnalysisRule实现

KeywordAnalysisRule::KeywordAnalysisRule(const std::string& name,
                                      const std::vector<std::string>& keywords,
                                      bool scoring)
    : name_(name), keywords_(keywords), scoring_(scoring) {
}

std::string KeywordAnalysisRule::GetName() const {
    return name_;
}

std::unordered_map<std::string, std::string> KeywordAnalysisRule::Analyze(const LogRecord& record) const {
    std::unordered_map<std::string, std::string> results;
    
    if (!config_.enabled) {
        results["enabled"] = "false";
        return results;
    }
    
    try {
        // 将日志消息转为小写以进行不区分大小写的匹配
        std::string lowerMessage = record.message;
        std::transform(lowerMessage.begin(), lowerMessage.end(), lowerMessage.begin(),
                      [](unsigned char c) { return std::tolower(c); });
        
        // 匹配关键字
        int matchCount = 0;
        std::vector<std::string> matchedKeywords;
        
        for (const auto& keyword : keywords_) {
            std::string lowerKeyword = keyword;
            std::transform(lowerKeyword.begin(), lowerKeyword.end(), lowerKeyword.begin(),
                          [](unsigned char c) { return std::tolower(c); });
            
            if (lowerMessage.find(lowerKeyword) != std::string::npos) {
                matchCount++;
                matchedKeywords.push_back(keyword);
            }
        }
        
        if (matchCount > 0) {
            results["matched"] = "true";
            results["rule"] = name_;
            results["group"] = config_.group;
            results["match_count"] = std::to_string(matchCount);
            
            if (scoring_ && !keywords_.empty()) {
                int score = (matchCount * 100) / static_cast<int>(keywords_.size());
                results["score"] = std::to_string(score);
            }
            
            std::stringstream ss;
            for (size_t i = 0; i < matchedKeywords.size(); ++i) {
                if (i > 0) ss << ", ";
                ss << matchedKeywords[i];
            }
            results["matched_keywords"] = ss.str();
        } else {
            results["matched"] = "false";
            results["group"] = config_.group;
        }
    } catch (const std::exception& e) {
        results["error"] = "分析错误: " + std::string(e.what());
    }
    
    return results;
}

const RuleConfig& KeywordAnalysisRule::GetConfig() const {
    return config_;
}

void KeywordAnalysisRule::SetConfig(const RuleConfig& config) {
    config_ = config;
}

void KeywordAnalysisRule::Enable() {
    config_.enabled = true;
}

void KeywordAnalysisRule::Disable() {
    config_.enabled = false;
}

bool KeywordAnalysisRule::IsEnabled() const {
    return config_.enabled;
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
        rules_.push_back(rule);
        
        // 添加到规则组
        const auto& group = rule->GetConfig().group;
        ruleGroups_[group].push_back(rule);
        
        // 按优先级排序
        SortRulesByPriority();
    }
}

void LogAnalyzer::SortRulesByPriority() {
    std::sort(rules_.begin(), rules_.end(),
              [](const auto& a, const auto& b) {
                  return a->GetConfig().priority > b->GetConfig().priority;
              });
              
    // 对每个规则组内的规则也进行排序
    for (auto& [group, rules] : ruleGroups_) {
        std::sort(rules.begin(), rules.end(),
                 [](const auto& a, const auto& b) {
                     return a->GetConfig().priority > b->GetConfig().priority;
                 });
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
    auto startTime = std::chrono::steady_clock::now();
    bool hasError = false;
    
    try {
        std::vector<std::shared_ptr<AnalysisRule>> rules;
        {
            std::lock_guard<std::mutex> lock(rulesMutex_);
            rules = rules_;  // 复制规则列表以避免处理过程中规则被修改
        }
        
        for (const auto& rule : rules) {
            if (!rule->IsEnabled()) {
                continue;
            }
            
            auto ruleStartTime = std::chrono::steady_clock::now();
            auto results = rule->Analyze(record);
            auto ruleEndTime = std::chrono::steady_clock::now();
            
            // 更新性能指标
            if (config_.enableMetrics) {
                auto processTime = std::chrono::duration_cast<std::chrono::microseconds>(
                    ruleEndTime - ruleStartTime);
                UpdateMetrics(rule->GetName(), processTime, results.count("error") > 0);
            }
            
            // 存储结果
            if (config_.storeResults) {
                if (redisStorage_) {
                    StoreResultToRedis(record.id, results);
                }
                if (mysqlStorage_) {
                    StoreResultToMySQL(record.id, results);
                }
            }
            
            // 调用回调函数
            {
                std::lock_guard<std::mutex> lock(callbackMutex_);
                if (analysisCallback_) {
                    analysisCallback_(record.id, results);
                }
            }
            
            if (results.count("error") > 0) {
                hasError = true;
            }
        }
    } catch (const std::exception& e) {
        hasError = true;
        std::cerr << "处理记录时发生错误: " << e.what() << std::endl;
    }
    
    // 更新总处理时间
    if (config_.enableMetrics) {
        auto endTime = std::chrono::steady_clock::now();
        auto totalTime = std::chrono::duration_cast<std::chrono::microseconds>(
            endTime - startTime);
        metrics_.totalProcessTime += totalTime.count();
        
        if (hasError) {
            metrics_.errorRecords++;
        }
    }
}

void LogAnalyzer::UpdateMetrics(const std::string& ruleName,
                              const std::chrono::microseconds& processTime,
                              bool hasError) {
    auto& ruleMetrics = metrics_.GetRuleMetrics(ruleName);
    ruleMetrics.processTime += processTime.count();
    ruleMetrics.matchCount++;
    if (hasError) {
        ruleMetrics.errorCount++;
    }
    ruleMetrics.lastMatchTime = std::chrono::steady_clock::now();
}

const AnalyzerMetrics& LogAnalyzer::GetMetrics() const {
    return metrics_;
}

void LogAnalyzer::ResetMetrics() {
    metrics_.Reset();
}

std::vector<std::string> LogAnalyzer::GetRuleGroups() const {
    std::vector<std::string> groups;
    std::lock_guard<std::mutex> lock(rulesMutex_);
    for (const auto& [group, _] : ruleGroups_) {
        groups.push_back(group);
    }
    return groups;
}

std::vector<std::shared_ptr<AnalysisRule>> LogAnalyzer::GetRulesByGroup(
    const std::string& group) const {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    auto it = ruleGroups_.find(group);
    if (it != ruleGroups_.end()) {
        return it->second;
    }
    return {};
}

void LogAnalyzer::EnableGroup(const std::string& group) {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    auto it = ruleGroups_.find(group);
    if (it != ruleGroups_.end()) {
        for (auto& rule : it->second) {
            rule->Enable();
        }
    }
}

void LogAnalyzer::DisableGroup(const std::string& group) {
    std::lock_guard<std::mutex> lock(rulesMutex_);
    auto it = ruleGroups_.find(group);
    if (it != ruleGroups_.end()) {
        for (auto& rule : it->second) {
            rule->Disable();
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