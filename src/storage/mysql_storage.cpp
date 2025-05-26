#include "xumj/storage/mysql_storage.h"
#include <cstdio>
#include <sstream>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <cstring>
#include <uuid/uuid.h>

namespace xumj {
namespace storage {

// 生成UUID
std::string GenerateUUID() {
    uuid_t uuid;
    uuid_generate(uuid);
    char uuid_str[37];
    uuid_unparse_lower(uuid, uuid_str);
    return std::string(uuid_str);
}

// ---------- MySQLConnection 实现 ----------

MySQLConnection::MySQLConnection(const MySQLConfig& config)
    : mysql_(nullptr), config_(config) {
    // 打印连接参数（调试用）
    std::cout << "MySQL连接参数:" << std::endl
              << "  host: " << config.host << std::endl
              << "  port: " << config.port << std::endl
              << "  username: " << config.username << std::endl
              << "  password: " << (config.password.empty() ? "空" : "已设置") << std::endl
              << "  database: " << config.database << std::endl;
    
    // 初始化MySQL库
    mysql_ = mysql_init(nullptr);
    if (mysql_ == nullptr) {
        throw MySQLStorageException("无法初始化MySQL连接: 内存不足");
    }
    
    // 设置连接超时,如果超时了，则自动终端并返回错误,而不是无限期地等待
    unsigned int timeout = config.timeout;
    mysql_options(mysql_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    
    // 连接MySQL服务器
    if (!mysql_real_connect(
            mysql_,
            config.host.c_str(),
            config.username.c_str(),
            config.password.c_str(),
            config.database.c_str(),
            config.port,
            nullptr,  // unix_socket
            0         // client_flag
        )) {
        std::string error = "MySQL连接失败: ";
        error += mysql_error(mysql_);
        mysql_close(mysql_);
        mysql_ = nullptr;
        throw MySQLStorageException(error);
    }
    
    // 设置字符集为UTF8
    mysql_set_character_set(mysql_, "utf8mb4");
}

MySQLConnection::~MySQLConnection() {
    if (mysql_ != nullptr) {
        mysql_close(mysql_);
        mysql_ = nullptr;
    }
}

int MySQLConnection::Execute(const std::string& sql) {
    if (!IsValid()) {
        if (!Reconnect()) {
            throw MySQLStorageException("MySQL连接已断开且无法重连");
        }
    }
    
    if (mysql_query(mysql_, sql.c_str()) != 0) {
        std::string error = "MySQL执行失败: ";
        error += mysql_error(mysql_);
        throw MySQLStorageException(error);
    }
    
    return mysql_affected_rows(mysql_);
}

std::vector<std::unordered_map<std::string, std::string>> MySQLConnection::Query(const std::string& sql) {
    if (!IsValid()) {
        if (!Reconnect()) {
            throw MySQLStorageException("MySQL连接已断开且无法重连");
        }
    }
    
    if (mysql_query(mysql_, sql.c_str()) != 0) {
        std::string error = "MySQL查询失败: ";
        error += mysql_error(mysql_);
        throw MySQLStorageException(error);
    }
    
    MYSQL_RES* result = mysql_store_result(mysql_);
    if (result == nullptr) {
        // 可能是非查询语句，或者出错
        if (mysql_field_count(mysql_) == 0) {
            // 非查询语句，返回空结果集
            return {};
        } else {
            // 出错
            std::string error = "MySQL获取结果集失败: ";
            error += mysql_error(mysql_);
            throw MySQLStorageException(error);
        }
    }
    
    // 获取列信息
    int num_fields = mysql_num_fields(result);
    MYSQL_FIELD* fields = mysql_fetch_fields(result);
    std::vector<std::string> field_names;
    for (int i = 0; i < num_fields; ++i) {
        field_names.push_back(fields[i].name);
    }
    
    // 获取数据
    std::vector<std::unordered_map<std::string, std::string>> rows;
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(result))) {
        std::unordered_map<std::string, std::string> row_map;
        for (int i = 0; i < num_fields; ++i) {
            if (row[i] != nullptr) {
                row_map[field_names[i]] = row[i];
            } else {
                row_map[field_names[i]] = "";
            }
        }
        rows.push_back(row_map);
    }
    
    mysql_free_result(result);
    return rows;
}

uint64_t MySQLConnection::GetLastInsertId() {
    return mysql_insert_id(mysql_);
}

void MySQLConnection::BeginTransaction() {
    Execute("START TRANSACTION");
}

void MySQLConnection::Commit() {
    Execute("COMMIT");
}

void MySQLConnection::Rollback() {
    Execute("ROLLBACK");
}

std::string MySQLConnection::EscapeString(const std::string& str) {
    size_t str_length = str.length();
    char* buffer = new char[str_length * 2 + 1];
    mysql_real_escape_string(mysql_, buffer, str.c_str(), str_length);
    std::string escaped_str(buffer);
    delete[] buffer;
    return escaped_str;
}

bool MySQLConnection::IsValid() const {
    return mysql_ != nullptr && mysql_ping(mysql_) == 0;
}

bool MySQLConnection::Reconnect() {
    if (mysql_ != nullptr) {
        mysql_close(mysql_);
    }
    
    // 重新初始化
    mysql_ = mysql_init(nullptr);
    if (mysql_ == nullptr) {
        return false;
    }
    
    // 设置连接超时
    unsigned int timeout = config_.timeout;
    mysql_options(mysql_, MYSQL_OPT_CONNECT_TIMEOUT, &timeout);
    
    // 连接MySQL服务器
    if (!mysql_real_connect(
            mysql_,
            config_.host.c_str(),
            config_.username.c_str(),
            config_.password.c_str(),
            config_.database.c_str(),
            config_.port,
            nullptr,  // unix_socket
            0         // client_flag
        )) {
        mysql_close(mysql_);
        mysql_ = nullptr;
        return false;
    }
    
    // 设置字符集为UTF8
    mysql_set_character_set(mysql_, "utf8mb4");
    
    return true;
}

// ---------- MySQLConnectionPool 实现 ----------

MySQLConnectionPool::MySQLConnectionPool(const MySQLConfig& config)
    : config_(config) {
    // 初始化连接池
    for (int i = 0; i < config.poolSize; ++i) {
        try {
            auto conn = CreateConnection();
            pool_.push_back(conn);
        } catch (const MySQLStorageException& e) {
            std::cerr << "初始化MySQL连接池失败: " << e.what() << std::endl;
            // 继续尝试创建其他连接
        }
    }
    
    if (pool_.empty()) {
        throw MySQLStorageException("无法创建任何MySQL连接");
    }
}

MySQLConnectionPool::~MySQLConnectionPool() {
    // 连接由智能指针管理，会自动释放
    pool_.clear();
}

std::shared_ptr<MySQLConnection> MySQLConnectionPool::GetConnection() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果池中有可用连接，则返回第一个可用连接
    for (auto it = pool_.begin(); it != pool_.end(); ++it) {
        if ((*it)->IsValid()) {
            auto conn = *it;
            pool_.erase(it);
            
            // 创建一个新的连接放回池中，保持池的大小
            try {
                pool_.push_back(CreateConnection());
            } catch (const MySQLStorageException& e) {
                std::cerr << "无法补充MySQL连接: " << e.what() << std::endl;
                // 继续，返回已获取的连接
            }
            
            return conn;
        }
    }
    
    // 如果没有可用连接，尝试创建一个新的连接
    return CreateConnection();
}

void MySQLConnectionPool::SetPoolSize(int size) {
    if (size <= 0) {
        throw MySQLStorageException("连接池大小必须大于0");
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 调整连接池大小
    if (size > static_cast<int>(pool_.size())) {
        // 增加连接
        for (int i = static_cast<int>(pool_.size()); i < size; ++i) {
            try {
                pool_.push_back(CreateConnection());
            } catch (const MySQLStorageException& e) {
                std::cerr << "无法增加MySQL连接: " << e.what() << std::endl;
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

int MySQLConnectionPool::GetPoolSize() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(pool_.size());
}

std::shared_ptr<MySQLConnection> MySQLConnectionPool::CreateConnection() {
    return std::make_shared<MySQLConnection>(config_);
}

// ---------- MySQLStorage 实现 ----------

MySQLStorage::MySQLStorage(const MySQLConfig& config)
    : pool_(std::make_unique<MySQLConnectionPool>(config)) {
}

MySQLStorage::~MySQLStorage() {
    // 连接池由智能指针管理，会自动释放
}

bool MySQLStorage::Initialize() {
    auto conn = pool_->GetConnection();
    
    try {
        // 创建日志表
        conn->Execute(
            "CREATE TABLE IF NOT EXISTS log_entries ("
            "    id VARCHAR(36) PRIMARY KEY,"
            "    timestamp DATETIME NOT NULL,"
            "    level VARCHAR(20) NOT NULL,"
            "    source VARCHAR(100) NOT NULL,"
            "    message TEXT NOT NULL,"
            "    INDEX idx_timestamp (timestamp),"
            "    INDEX idx_level (level),"
            "    INDEX idx_source (source),"
            "    FULLTEXT INDEX idx_message (message)"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;"
        );
        
        // 创建日志自定义字段表
        conn->Execute(
            "CREATE TABLE IF NOT EXISTS log_fields ("
            "    log_id VARCHAR(36) NOT NULL,"
            "    field_name VARCHAR(50) NOT NULL,"
            "    field_value TEXT NOT NULL,"
            "    PRIMARY KEY (log_id, field_name),"
            "    FOREIGN KEY (log_id) REFERENCES log_entries(id) ON DELETE CASCADE,"
            "    INDEX idx_field_name (field_name)"
            ") ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;"
        );
        
        return true;
    } catch (const MySQLStorageException& e) {
        std::cerr << "初始化数据库表结构失败: " << e.what() << std::endl;
        return false;
    }
}

bool MySQLStorage::SaveLogEntry(const LogEntry& entry) {
    try {
        // 首先检查连接是否可用
        if (!TestConnection()) {
            std::cerr << "保存日志条目失败: MySQL连接不可用" << std::endl;
            // 尝试重新初始化
            if (Initialize()) {
                std::cout << "MySQL连接已重新初始化" << std::endl;
            } else {
                std::cerr << "MySQL连接重新初始化失败" << std::endl;
                return false;
            }
        }
        
        auto conn = pool_->GetConnection();
        if (!conn) {
            std::cerr << "保存日志条目失败: 无法获取数据库连接" << std::endl;
            return false;
        }
        
        try {
            // 开始事务
            conn->BeginTransaction();
            
            // 生成UUID作为日志ID（如果没有提供）
            std::string id = entry.id.empty() ? GenerateUUID() : entry.id;
            
            // 检查ID是否已存在
            std::stringstream check_sql;
            check_sql << "SELECT COUNT(*) as count FROM log_entries WHERE id = '"
                      << conn->EscapeString(id) << "'";
            
            auto result = conn->Query(check_sql.str());
            if (!result.empty() && result[0].find("count") != result[0].end()) {
                int count = std::stoi(result[0]["count"]);
                if (count > 0) {
                    std::cout << "日志ID已存在，跳过插入: " << id << std::endl;
                    conn->Rollback();
                    return true; // 已存在视为成功
                }
            }
            
            // 预处理消息文本，处理特殊字符和过长内容
            std::string safeMessage = entry.message;
            // 截断过长消息
            if (safeMessage.size() > 65535) {
                safeMessage = safeMessage.substr(0, 65530) + "...";
            }
            
            // 安全处理时间戳格式
            std::string safeTimestamp = entry.timestamp;
            if (safeTimestamp.empty()) {
                // 使用当前时间
                auto now = std::chrono::system_clock::now();
                auto now_time_t = std::chrono::system_clock::to_time_t(now);
                std::tm now_tm = *std::localtime(&now_time_t);
                
                std::stringstream ss;
                ss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
                safeTimestamp = ss.str();
            } else if (safeTimestamp.find("-") == std::string::npos || safeTimestamp.find(":") == std::string::npos) {
                // 尝试将时间戳转换为标准格式
                try {
                    std::time_t timestamp = std::stoul(safeTimestamp);
                    std::tm now_tm = *std::localtime(&timestamp);

                    std::stringstream ss;
                    ss<<std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
                    safeTimestamp = ss.str();
                } catch (...) {
                    // 转换失败，使用当前时间
                    auto now = std::chrono::system_clock::now();
                    auto now_time_t = std::chrono::system_clock::to_time_t(now);
                    std::tm now_tm = *std::localtime(&now_time_t);
                    
                    std::stringstream ss;
                    ss << std::put_time(&now_tm, "%Y-%m-%d %H:%M:%S");
                    safeTimestamp = ss.str();
                }
            }
            
            // 限制级别和来源的长度
            std::string safeLevel = entry.level.empty() ? "INFO" : entry.level;
            if (safeLevel.size() > 20) {
                safeLevel = safeLevel.substr(0, 20);
            }
            
            std::string safeSource = entry.source.empty() ? "unknown" : entry.source;
            if (safeSource.size() > 100) {
                safeSource = safeSource.substr(0, 100);
            }
            
            // 插入日志条目
            std::string insert_sql = "INSERT INTO log_entries (id, timestamp, level, source, message) VALUES (?, ?, ?, ?, ?)";
            
            // 使用预处理语句
            MYSQL_STMT* stmt = mysql_stmt_init(conn->GetNativeHandle());
            if (!stmt) {
                std::string error = "MySQL创建预处理语句失败: ";
                error += mysql_error(conn->GetNativeHandle());
                throw MySQLStorageException(error);
            }
            
            if (mysql_stmt_prepare(stmt, insert_sql.c_str(), insert_sql.length()) != 0) {
                std::string error = "MySQL预处理语句准备失败: ";
                error += mysql_stmt_error(stmt);
                mysql_stmt_close(stmt);
                throw MySQLStorageException(error);
            }
            
            // 绑定参数
            MYSQL_BIND bind[5];
            memset(bind, 0, sizeof(bind));
            
            bind[0].buffer_type = MYSQL_TYPE_STRING;
            bind[0].buffer = (void*)id.c_str();
            bind[0].buffer_length = id.length();
            
            bind[1].buffer_type = MYSQL_TYPE_STRING;
            bind[1].buffer = (void*)safeTimestamp.c_str();
            bind[1].buffer_length = safeTimestamp.length();
            
            bind[2].buffer_type = MYSQL_TYPE_STRING;
            bind[2].buffer = (void*)safeLevel.c_str();
            bind[2].buffer_length = safeLevel.length();
            
            bind[3].buffer_type = MYSQL_TYPE_STRING;
            bind[3].buffer = (void*)safeSource.c_str();
            bind[3].buffer_length = safeSource.length();
            
            bind[4].buffer_type = MYSQL_TYPE_STRING;
            bind[4].buffer = (void*)safeMessage.c_str();
            bind[4].buffer_length = safeMessage.length();
            
            if (mysql_stmt_bind_param(stmt, bind) != 0) {
                std::string error = "MySQL绑定参数失败: ";
                error += mysql_stmt_error(stmt);
                mysql_stmt_close(stmt);
                throw MySQLStorageException(error);
            }
            
            if (mysql_stmt_execute(stmt) != 0) {
                std::string error = "MySQL执行预处理语句失败: ";
                error += mysql_stmt_error(stmt);
                mysql_stmt_close(stmt);
                throw MySQLStorageException(error);
            }
            
            mysql_stmt_close(stmt);
            
            // 插入自定义字段
            int fieldCount = 0;
            const int maxFields = 20; // 限制字段数量
            
            for (const auto& field : entry.fields) {
                if (fieldCount >= maxFields) {
                    std::cout << "已达到最大字段数量限制 (" << maxFields << ")，忽略剩余字段" << std::endl;
                    break;
                }
                
                // 安全处理字段名和值
                std::string safeFieldName = field.first;
                if (safeFieldName.size() > 50) {
                    safeFieldName = safeFieldName.substr(0, 50);
                }
                
                std::string safeFieldValue = field.second;
                if (safeFieldValue.size() > 65535) {
                    safeFieldValue = safeFieldValue.substr(0, 65530) + "...";
                }
                
                std::stringstream field_sql;
                field_sql << "INSERT INTO log_fields (log_id, field_name, field_value) VALUES ("
                    << "'" << conn->EscapeString(id) << "', "
                    << "'" << conn->EscapeString(safeFieldName) << "', "
                    << "'" << conn->EscapeString(safeFieldValue) << "')";
                
                try {
                    conn->Execute(field_sql.str());
                    fieldCount++;
                } catch (const MySQLStorageException& e) {
                    std::cerr << "插入字段失败 (" << safeFieldName << "): " << e.what() << std::endl;
                    // 继续处理其他字段
                }
            }
            
            // 提交事务
            conn->Commit();
            return true;
        } catch (const MySQLStorageException& e) {
            // 回滚事务
            try {
                conn->Rollback();
            } catch (...) {
                // 忽略回滚错误
            }
            
            std::cerr << "保存日志条目失败: " << e.what() << std::endl;
            return false;
        } catch (const std::exception& e) {
            // 回滚事务
            try {
                conn->Rollback();
            } catch (...) {
                // 忽略回滚错误
            }
            
            std::cerr << "保存日志条目时发生未知错误: " << e.what() << std::endl;
            return false;
        }
    } catch (const std::exception& e) {
        std::cerr << "保存日志条目过程中发生异常: " << e.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "保存日志条目过程中发生未知异常" << std::endl;
        return false;
    }
}

int MySQLStorage::SaveLogEntries(const std::vector<LogEntry>& entries) {
    if (entries.empty()) {
        return 0;
    }
    
    auto conn = pool_->GetConnection();
    int success_count = 0;
    
    try {
        // 开始事务
        conn->BeginTransaction();
        
        // 批量插入日志条目
        for (const auto& entry : entries) {
            // 生成UUID作为日志ID（如果没有提供）
            std::string id = entry.id.empty() ? GenerateUUID() : entry.id;
            
            // 插入日志条目
            std::stringstream sql;
            sql << "INSERT INTO log_entries (id, timestamp, level, source, message) VALUES ("
                << "'" << conn->EscapeString(id) << "', "
                << "'" << conn->EscapeString(entry.timestamp) << "', "
                << "'" << conn->EscapeString(entry.level) << "', "
                << "'" << conn->EscapeString(entry.source) << "', "
                << "'" << conn->EscapeString(entry.message) << "')";
            
            conn->Execute(sql.str());
            
            // 插入自定义字段
            for (const auto& field : entry.fields) {
                std::stringstream field_sql;
                field_sql << "INSERT INTO log_fields (log_id, field_name, field_value) VALUES ("
                    << "'" << conn->EscapeString(id) << "', "
                    << "'" << conn->EscapeString(field.first) << "', "
                    << "'" << conn->EscapeString(field.second) << "')";
                
                conn->Execute(field_sql.str());
            }
            
            ++success_count;
        }
        
        // 提交事务
        conn->Commit();
        return success_count;
    } catch (const MySQLStorageException& e) {
        // 回滚事务
        try {
            conn->Rollback();
        } catch (...) {
            // 忽略回滚错误
        }
        
        std::cerr << "批量保存日志条目失败: " << e.what() << std::endl;
        return success_count;
    }
}

std::vector<MySQLStorage::LogEntry> MySQLStorage::QueryLogEntries(
    const std::unordered_map<std::string, std::string>& conditions,
    int limit,
    int offset
) {
    auto conn = pool_->GetConnection();
    
    try {
        // 构建查询SQL
        std::stringstream sql;
        sql << "SELECT * FROM log_entries";
        
        // 添加WHERE子句
        std::string where_clause = BuildWhereClause(conditions);
        if (!where_clause.empty()) {
            sql << " WHERE " << where_clause;
        }
        
        // 添加ORDER BY子句
        sql << " ORDER BY timestamp DESC";
        
        // 添加LIMIT和OFFSET子句
        if (limit > 0) {
            sql << " LIMIT " << limit;
            if (offset > 0) {
                sql << " OFFSET " << offset;
            }
        }
        
        // 执行查询
        auto rows = conn->Query(sql.str());
        
        // 转换结果
        std::vector<LogEntry> entries;
        for (const auto& row : rows) {
            entries.push_back(RowToLogEntry(row));
        }
        
        return entries;
    } catch (const MySQLStorageException& e) {
        std::cerr << "查询日志条目失败: " << e.what() << std::endl;
        return {};
    }
}

MySQLStorage::LogEntry MySQLStorage::GetLogEntryById(const std::string& id) {
    auto conn = pool_->GetConnection();
    
    try {
        // 查询日志条目
        std::stringstream sql;
        sql << "SELECT * FROM log_entries WHERE id = '" << conn->EscapeString(id) << "'";
        
        auto rows = conn->Query(sql.str());
        if (rows.empty()) {
            // 没有找到日志条目
            return LogEntry();
        }
        
        // 转换结果
        return RowToLogEntry(rows[0]);
    } catch (const MySQLStorageException& e) {
        std::cerr << "获取日志条目失败: " << e.what() << std::endl;
        return LogEntry();
    }
}

std::vector<MySQLStorage::LogEntry> MySQLStorage::QueryLogEntriesByTimeRange(
    const std::string& startTime,
    const std::string& endTime,
    int limit,
    int offset
) {
    std::unordered_map<std::string, std::string> conditions;
    if (!startTime.empty() && !endTime.empty()) {
        conditions["timestamp_range"] = startTime + " TO " + endTime;
    } else if (!startTime.empty()) {
        conditions["timestamp_min"] = startTime;
    } else if (!endTime.empty()) {
        conditions["timestamp_max"] = endTime;
    }
    
    return QueryLogEntries(conditions, limit, offset);
}

std::vector<MySQLStorage::LogEntry> MySQLStorage::QueryLogEntriesByLevel(
    const std::string& level,
    int limit,
    int offset
) {
    std::unordered_map<std::string, std::string> conditions;
    conditions["level"] = level;
    
    return QueryLogEntries(conditions, limit, offset);
}

std::vector<MySQLStorage::LogEntry> MySQLStorage::QueryLogEntriesBySource(
    const std::string& source,
    int limit,
    int offset
) {
    std::unordered_map<std::string, std::string> conditions;
    conditions["source"] = source;
    
    return QueryLogEntries(conditions, limit, offset);
}

std::vector<MySQLStorage::LogEntry> MySQLStorage::SearchLogEntriesByKeyword(
    const std::string& keyword,
    int limit,
    int offset
) {
    auto conn = pool_->GetConnection();
    
    try {
        // 构建搜索查询 - 使用LIKE代替MATCH AGAINST进行简单搜索
        std::stringstream sql;
        sql << "SELECT * FROM log_entries WHERE message LIKE '%"
            << conn->EscapeString(keyword) << "%'";
        
        // 添加ORDER BY子句
        sql << " ORDER BY timestamp DESC";
        
        // 添加LIMIT和OFFSET子句
        if (limit > 0) {
            sql << " LIMIT " << limit;
            if (offset > 0) {
                sql << " OFFSET " << offset;
            }
        }
        
        // 执行查询
        auto rows = conn->Query(sql.str());
        
        // 转换结果
        std::vector<LogEntry> entries;
        for (const auto& row : rows) {
            entries.push_back(RowToLogEntry(row));
        }
        
        return entries;
    } catch (const MySQLStorageException& e) {
        std::cerr << "搜索日志条目失败: " << e.what() << std::endl;
        return {};
    }
} 

int MySQLStorage::GetLogEntryCount() {
    auto conn = pool_->GetConnection();
    
    try {
        // 查询总数
        auto rows = conn->Query("SELECT COUNT(*) AS count FROM log_entries");
        if (rows.empty()) {
            return 0;
        }
        
        return std::stoi(rows[0]["count"]);
    } catch (const MySQLStorageException& e) {
        std::cerr << "获取日志条目总数失败: " << e.what() << std::endl;
        return 0;
    }
}

int MySQLStorage::DeleteLogEntriesBefore(const std::string& beforeTime) {
    auto conn = pool_->GetConnection();
    
    try {
        // 删除指定时间之前的日志条目
        std::stringstream sql;
        sql << "DELETE FROM log_entries WHERE timestamp < '" << conn->EscapeString(beforeTime) << "'";
        
        return conn->Execute(sql.str());
    } catch (const MySQLStorageException& e) {
        std::cerr << "删除过期日志条目失败: " << e.what() << std::endl;
        return 0;
    }
}

bool MySQLStorage::TestConnection() {
    try {
        auto conn = pool_->GetConnection();
        conn->Query("SELECT 1");
        return true;
    } catch (const MySQLStorageException&) {
        return false;
    }
}

MySQLStorage::LogEntry MySQLStorage::RowToLogEntry(const std::unordered_map<std::string, std::string>& row) {
    LogEntry entry;
    
    // 获取基本字段
    if (row.count("id")) {
        entry.id = row.at("id");
    }
    
    if (row.count("timestamp")) {
        entry.timestamp = row.at("timestamp");
    }
    
    if (row.count("level")) {
        entry.level = row.at("level");
    }
    
    if (row.count("source")) {
        entry.source = row.at("source");
    }
    
    if (row.count("message")) {
        entry.message = row.at("message");
    }
    
    // 获取自定义字段
    if (!entry.id.empty()) {
        entry.fields = GetLogEntryFields(entry.id);
    }
    
    return entry;
}

std::string MySQLStorage::BuildWhereClause(const std::unordered_map<std::string, std::string>& conditions) {
    auto conn = pool_->GetConnection();
    std::vector<std::string> clauses;
    
    for (const auto& condition : conditions) {
        std::string field = condition.first;
        std::string value = condition.second;
         // 特殊处理时间范围查询
        if (field == "timestamp_range") {
            // 格式: "开始时间 TO 结束时间"
            size_t pos = value.find(" TO ");
            if (pos != std::string::npos) {
                std::string start_time = value.substr(0, pos);
                std::string end_time = value.substr(pos + 4);
                
                // 构建时间范围条件
                clauses.push_back("timestamp >= '" + conn->EscapeString(start_time) + "' AND timestamp <= '" + conn->EscapeString(end_time) + "'");
            }
        } else if (field == "timestamp_min") {
            // 处理最小时间条件
            clauses.push_back("timestamp >= '" + conn->EscapeString(value) + "'");
        } else if (field == "timestamp_max") {
            // 处理最大时间条件
            clauses.push_back("timestamp <= '" + conn->EscapeString(value) + "'");
        } else {
            // 处理普通字段条件
            clauses.push_back(field + " = '" + conn->EscapeString(value) + "'");
        }
    }
    
    if (clauses.empty()) {
        return "";
    }
    
    // 将所有条件用AND连接
    std::string result = clauses[0];
    for (size_t i = 1; i < clauses.size(); ++i) {
        result += " AND " + clauses[i];
    }
    
    return result;
}

std::unordered_map<std::string, std::string> MySQLStorage::GetLogEntryFields(const std::string& logId) {
    auto conn = pool_->GetConnection();
    
    try {
        // 查询自定义字段
        std::stringstream sql;
        sql << "SELECT field_name, field_value FROM log_fields WHERE log_id = '" << conn->EscapeString(logId) << "'";
        
        auto rows = conn->Query(sql.str());
        
        // 构建字段映射
        std::unordered_map<std::string, std::string> fields;
        for (const auto& row : rows) {
            fields[row.at("field_name")] = row.at("field_value");
        }
        
        return fields;
    } catch (const MySQLStorageException& e) {
        std::cerr << "获取日志条目字段失败: " << e.what() << std::endl;
        return {};
    }
}

} // namespace storage
} // namespace xumj 