#include "xumj/storage/redis_storage.h"
#include <cstdarg>
#include <sstream>
#include <iostream>

namespace xumj {
namespace storage {

// ---------- RedisConnection 实现 ----------

RedisConnection::RedisConnection(const RedisConfig& config)
    : context_(nullptr), config_(config) {
    // 连接Redis服务器
    struct timeval timeout = {
        config.timeout / 1000,       // 秒
        (config.timeout % 1000) * 1000 // 微秒
    };
    
    context_ = redisConnectWithTimeout(config.host.c_str(), config.port, timeout);
    
    if (context_ == nullptr || context_->err) {
        std::string error = "Redis连接失败";
        if (context_ != nullptr) {
            error += ": " + std::string(context_->errstr);
            redisFree(context_);
            context_ = nullptr;
        } else {
            error += ": 无法分配Redis上下文";
        }
        throw RedisStorageException(error);
    }
    
    // 如果有密码，进行认证
    if (!config.password.empty()) {
        redisReply* reply = (redisReply*)redisCommand(context_, "AUTH %s", config.password.c_str());
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            std::string error = "Redis认证失败";
            if (reply != nullptr) {
                error += ": " + std::string(reply->str);
                freeReplyObject(reply);
            }
            redisFree(context_);
            context_ = nullptr;
            throw RedisStorageException(error);
        }
        freeReplyObject(reply);
    }
    
    // 选择数据库
    if (config.database != 0) {
        redisReply* reply = (redisReply*)redisCommand(context_, "SELECT %d", config.database);
        if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
            std::string error = "Redis选择数据库失败";
            if (reply != nullptr) {
                error += ": " + std::string(reply->str);
                freeReplyObject(reply);
            }
            redisFree(context_);
            context_ = nullptr;
            throw RedisStorageException(error);
        }
        freeReplyObject(reply);
    }
}

RedisConnection::~RedisConnection() {
    if (context_ != nullptr) {
        redisFree(context_);
        context_ = nullptr;
    }
}

redisReply* RedisConnection::Execute(const char* format, ...) {
    if (!IsValid()) {
        if (!Reconnect()) {
            throw RedisStorageException("Redis连接已断开且无法重连");
        }
    }
    
    va_list ap;
    va_start(ap, format);
    redisReply* reply = (redisReply*)redisvCommand(context_, format, ap);
    va_end(ap);
    
    if (reply == nullptr) {
        // 连接可能已断开
        if (context_->err) {
            std::string error = "Redis命令执行失败: " + std::string(context_->errstr);
            throw RedisStorageException(error);
        }
        throw RedisStorageException("Redis命令执行失败: 未知错误");
    }
    
    if (reply->type == REDIS_REPLY_ERROR) {
        std::string error = "Redis错误: " + std::string(reply->str);
        freeReplyObject(reply);
        throw RedisStorageException(error);
    }
    
    return reply;
}

redisReply* RedisConnection::ExecuteArgv(int argc, const char** argv, const size_t* argvlen) {
    if (!IsValid()) {
        if (!Reconnect()) {
            throw RedisStorageException("Redis连接已断开且无法重连");
        }
    }
    
    redisReply* reply = (redisReply*)redisCommandArgv(context_, argc, argv, argvlen);
    
    if (reply == nullptr) {
        // 连接可能已断开
        if (context_->err) {
            std::string error = "Redis命令执行失败: " + std::string(context_->errstr);
            throw RedisStorageException(error);
        }
        throw RedisStorageException("Redis命令执行失败: 未知错误");
    }
    
    if (reply->type == REDIS_REPLY_ERROR) {
        std::string error = "Redis错误: " + std::string(reply->str);
        freeReplyObject(reply);
        throw RedisStorageException(error);
    }
    
    return reply;
}

bool RedisConnection::IsValid() const {
    return context_ != nullptr && !context_->err;
}

bool RedisConnection::Reconnect() {
    if (context_ != nullptr) {
        redisFree(context_);
        context_ = nullptr;
    }
    
    try {
        struct timeval timeout = {
            config_.timeout / 1000,       // 秒
            (config_.timeout % 1000) * 1000 // 微秒
        };
        
        context_ = redisConnectWithTimeout(config_.host.c_str(), config_.port, timeout);
        
        if (context_ == nullptr || context_->err) {
            if (context_ != nullptr) {
                redisFree(context_);
                context_ = nullptr;
            }
            return false;
        }
        
        // 如果有密码，进行认证
        if (!config_.password.empty()) {
            redisReply* reply = (redisReply*)redisCommand(context_, "AUTH %s", config_.password.c_str());
            if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
                if (reply != nullptr) {
                    freeReplyObject(reply);
                }
                redisFree(context_);
                context_ = nullptr;
                return false;
            }
            freeReplyObject(reply);
        }
        
        // 选择数据库
        if (config_.database != 0) {
            redisReply* reply = (redisReply*)redisCommand(context_, "SELECT %d", config_.database);
            if (reply == nullptr || reply->type == REDIS_REPLY_ERROR) {
                if (reply != nullptr) {
                    freeReplyObject(reply);
                }
                redisFree(context_);
                context_ = nullptr;
                return false;
            }
            freeReplyObject(reply);
        }
        
        return true;
    } catch (...) {
        if (context_ != nullptr) {
            redisFree(context_);
            context_ = nullptr;
        }
        return false;
    }
}

// ---------- RedisConnectionPool 实现 ----------

RedisConnectionPool::RedisConnectionPool(const RedisConfig& config)
    : config_(config) {
    // 初始化连接池
    for (int i = 0; i < config.poolSize; ++i) {
        try {
            auto conn = CreateConnection();
            pool_.push_back(conn);
        } catch (const RedisStorageException& e) {
            std::cerr << "初始化Redis连接池失败: " << e.what() << std::endl;
            // 继续尝试创建其他连接
        }
    }
    
    if (pool_.empty()) {
        throw RedisStorageException("无法创建任何Redis连接");
    }
}

RedisConnectionPool::~RedisConnectionPool() {
    // 连接由智能指针管理，会自动释放
    pool_.clear();
}

std::shared_ptr<RedisConnection> RedisConnectionPool::GetConnection() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果池中有可用连接，则返回第一个可用连接
    for (auto it = pool_.begin(); it != pool_.end(); ++it) {
        if ((*it)->IsValid()) {
            auto conn = *it;
            pool_.erase(it);
            
            // 创建一个新的连接放回池中，保持池的大小
            try {
                pool_.push_back(CreateConnection());
            } catch (const RedisStorageException& e) {
                std::cerr << "无法补充Redis连接: " << e.what() << std::endl;
                // 继续，返回已获取的连接
            }
            
            return conn;
        }
    }
    
    // 如果没有可用连接，尝试创建一个新的连接
    return CreateConnection();
}

void RedisConnectionPool::SetPoolSize(int size) {
    if (size <= 0) {
        throw RedisStorageException("连接池大小必须大于0");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 调整连接池大小
    if (size > static_cast<int>(pool_.size())) {
        // 增加连接
        for (int i = static_cast<int>(pool_.size()); i < size; ++i) {
            try {
                pool_.push_back(CreateConnection());
            } catch (const RedisStorageException& e) {
                std::cerr << "无法增加Redis连接: " << e.what() << std::endl;
                // 继续尝试创建其他连接
            }
        }
    } else if (size < static_cast<int>(pool_.size())) {
        // 减少连接
        pool_.resize(size);
    }
    
    // 更新配置
    config_.poolSize = size;
}

int RedisConnectionPool::GetPoolSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(pool_.size());
}

std::shared_ptr<RedisConnection> RedisConnectionPool::CreateConnection() {
    return std::make_shared<RedisConnection>(config_);
}

// ---------- RedisStorage 实现 ----------

RedisStorage::RedisStorage(const RedisConfig& config)
    : pool_(std::make_unique<RedisConnectionPool>(config)) {
}

RedisStorage::~RedisStorage() {
    // 连接池由智能指针管理，会自动释放
}

std::string RedisStorage::ExecuteCommand(std::function<redisReply*(RedisConnection*)> command) {
    auto conn = pool_->GetConnection();
    redisReply* reply = nullptr;
    
    try {
        reply = command(conn.get());
        
        std::string result;
        
        switch (reply->type) {
            case REDIS_REPLY_STRING:
                result = reply->str;
                break;
            case REDIS_REPLY_INTEGER:
                result = std::to_string(reply->integer);
                break;
            case REDIS_REPLY_NIL:
                result = "";
                break;
            case REDIS_REPLY_STATUS:
                result = reply->str;
                break;
            case REDIS_REPLY_ERROR:
                throw RedisStorageException(std::string("Redis错误: ") + reply->str);
            case REDIS_REPLY_ARRAY: {
                std::stringstream ss;
                for (size_t i = 0; i < reply->elements; ++i) {
                    redisReply* element = reply->element[i];
                    if (element->type == REDIS_REPLY_STRING) {
                        if (i > 0) ss << "\n";
                        ss << element->str;
                    } else if (element->type == REDIS_REPLY_INTEGER) {
                        if (i > 0) ss << "\n";
                        ss << element->integer;
                    }
                }
                result = ss.str();
                break;
            }
            default:
                result = "";
        }
        
        freeReplyObject(reply);
        return result;
    } catch (const std::exception& e) {
        if (reply != nullptr) {
            freeReplyObject(reply);
        }
        throw;
    }
}

bool RedisStorage::Set(const std::string& key, const std::string& value, int expireSeconds) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        redisReply* reply;
        if (expireSeconds > 0) {
            reply = conn->Execute("SETEX %s %d %s", key.c_str(), expireSeconds, value.c_str());
        } else {
            reply = conn->Execute("SET %s %s", key.c_str(), value.c_str());
        }
        return reply;
    };
    
    std::string result = ExecuteCommand(command);
    return result == "OK";
}

std::string RedisStorage::Get(const std::string& key, const std::string& defaultValue) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("GET %s", key.c_str());
    };
    
    try {
        std::string result = ExecuteCommand(command);
        return result.empty() ? defaultValue : result;
    } catch (const RedisStorageException&) {
        return defaultValue;
    }
}

bool RedisStorage::Delete(const std::string& key) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("DEL %s", key.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    return result != "0";
}

bool RedisStorage::Exists(const std::string& key) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("EXISTS %s", key.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    return result != "0";
}

bool RedisStorage::Expire(const std::string& key, int expireSeconds) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("EXPIRE %s %d", key.c_str(), expireSeconds);
    };
    
    std::string result = ExecuteCommand(command);
    return result != "0";
}

int RedisStorage::ListPush(const std::string& key, const std::string& value) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("RPUSH %s %s", key.c_str(), value.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    return std::stoi(result);
}

int RedisStorage::ListPushFront(const std::string& key, const std::string& value) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("LPUSH %s %s", key.c_str(), value.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    return std::stoi(result);
}

std::string RedisStorage::ListPop(const std::string& key) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("RPOP %s", key.c_str());
    };
    
    try {
        return ExecuteCommand(command);
    } catch (const RedisStorageException&) {
        return "";
    }
}

std::string RedisStorage::ListPopFront(const std::string& key) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("LPOP %s", key.c_str());
    };
    
    try {
        return ExecuteCommand(command);
    } catch (const RedisStorageException&) {
        return "";
    }
}

int RedisStorage::ListLength(const std::string& key) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("LLEN %s", key.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    return std::stoi(result);
}

std::vector<std::string> RedisStorage::ListRange(const std::string& key, int start, int end) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("LRANGE %s %d %d", key.c_str(), start, end);
    };
    
    std::string result = ExecuteCommand(command);
    std::vector<std::string> elements;
    
    if (!result.empty()) {
        std::stringstream ss(result);
        std::string line;
        while (std::getline(ss, line)) {
            elements.push_back(line);
        }
    }
    
    return elements;
}

bool RedisStorage::HashSet(const std::string& key, const std::string& field, const std::string& value) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("HSET %s %s %s", key.c_str(), field.c_str(), value.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    return result == "1" || result == "0";  // 1表示新字段，0表示更新已有字段
}

std::string RedisStorage::HashGet(const std::string& key, const std::string& field, const std::string& defaultValue) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("HGET %s %s", key.c_str(), field.c_str());
    };
    
    try {
        std::string result = ExecuteCommand(command);
        return result.empty() ? defaultValue : result;
    } catch (const RedisStorageException&) {
        return defaultValue;
    }
}

bool RedisStorage::HashDelete(const std::string& key, const std::string& field) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("HDEL %s %s", key.c_str(), field.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    return result != "0";
}

bool RedisStorage::HashExists(const std::string& key, const std::string& field) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("HEXISTS %s %s", key.c_str(), field.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    return result != "0";
}

std::unordered_map<std::string, std::string> RedisStorage::HashGetAll(const std::string& key) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("HGETALL %s", key.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    std::unordered_map<std::string, std::string> hashMap;
    
    if (!result.empty()) {
        std::stringstream ss(result);
        std::string field, value;
        bool isField = true;
        
        std::string line;
        while (std::getline(ss, line)) {
            if (isField) {
                field = line;
            } else {
                value = line;
                hashMap[field] = value;
            }
            isField = !isField;
        }
    }
    
    return hashMap;
}

int RedisStorage::SetAdd(const std::string& key, const std::string& member) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("SADD %s %s", key.c_str(), member.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    return std::stoi(result);
}

int RedisStorage::SetRemove(const std::string& key, const std::string& member) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("SREM %s %s", key.c_str(), member.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    return std::stoi(result);
}

bool RedisStorage::SetIsMember(const std::string& key, const std::string& member) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("SISMEMBER %s %s", key.c_str(), member.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    return result != "0";
}

std::vector<std::string> RedisStorage::SetMembers(const std::string& key) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("SMEMBERS %s", key.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    std::vector<std::string> members;
    
    if (!result.empty()) {
        std::stringstream ss(result);
        std::string line;
        while (std::getline(ss, line)) {
            members.push_back(line);
        }
    }
    
    return members;
}

int RedisStorage::SetSize(const std::string& key) {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("SCARD %s", key.c_str());
    };
    
    std::string result = ExecuteCommand(command);
    return std::stoi(result);
}

bool RedisStorage::Multi() {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("MULTI");
    };
    
    std::string result = ExecuteCommand(command);
    return result == "OK";
}

std::vector<std::string> RedisStorage::Exec() {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("EXEC");
    };
    
    std::string result = ExecuteCommand(command);
    std::vector<std::string> results;
    
    if (!result.empty()) {
        std::stringstream ss(result);
        std::string line;
        while (std::getline(ss, line)) {
            results.push_back(line);
        }
    }
    
    return results;
}

bool RedisStorage::Discard() {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("DISCARD");
    };
    
    std::string result = ExecuteCommand(command);
    return result == "OK";
}

bool RedisStorage::Ping() {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("PING");
    };
    
    try {
        std::string result = ExecuteCommand(command);
        return result == "PONG";
    } catch (const RedisStorageException&) {
        return false;
    }
}

std::string RedisStorage::Info() {
    auto command = [&](RedisConnection* conn) -> redisReply* {
        return conn->Execute("INFO");
    };
    
    return ExecuteCommand(command);
}

} // namespace storage
} // namespace xumj 