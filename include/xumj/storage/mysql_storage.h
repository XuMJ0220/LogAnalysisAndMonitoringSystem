#ifndef XUMJ_STORAGE_MYSQL_STORAGE_H
#define XUMJ_STORAGE_MYSQL_STORAGE_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <mysql/mysql.h>

namespace xumj {
namespace storage {

/*
 * @class MySQLStorageException
 * @brief MySQL存储异常类
 */
class MySQLStorageException : public std::runtime_error {
public:
    explicit MySQLStorageException(const std::string& message)
        : std::runtime_error(message) {}
};

/*
 * @struct MySQLConfig
 * @brief MySQL连接配置
 */
struct MySQLConfig {
    std::string host{"127.0.0.1"};  // MySQL服务器主机名或IP
    int port{3306};                // MySQL服务器端口
    std::string username{"root"};  // MySQL用户名
    std::string password{};        // MySQL密码
    std::string database{};        // MySQL数据库名
    int timeout{5};                // 连接超时（秒）
    int poolSize{5};               // 连接池大小
};

/*
 * @class MySQLConnection
 * @brief MySQL连接封装
 */
class MySQLConnection {
public:
    /*
     * @brief 构造函数
     * @param config MySQL配置
     */
    explicit MySQLConnection(const MySQLConfig& config);
    
    /*
     * @brief 析构函数
     */
    ~MySQLConnection();
    
    /*
     * @brief 执行SQL语句（非查询）
     * @param sql SQL语句
     * @return 受影响的行数
     */
    int Execute(const std::string& sql);
    
    /*
     * @brief 执行SQL查询（返回结果集）
     * @param sql SQL语句
     * @return 结果集（行/列映射）
     */
    std::vector<std::unordered_map<std::string, std::string>> Query(const std::string& sql);
    
    /*
     * @brief 获取最后一次插入的自增ID
     * @return 自增ID
     */
    uint64_t GetLastInsertId();
    
    /*
     * @brief 开始事务
     */
    void BeginTransaction();
    
    /*
     * @brief 提交事务
     */
    void Commit();
    
    /*
     * @brief 回滚事务
     */
    void Rollback();
    
    /*
     * @brief 转义字符串，防止SQL注入
     * @param str 原始字符串
     * @return 转义后的字符串
     */
    std::string EscapeString(const std::string& str);
    
    /*
     * @brief 检查连接是否有效
     * @return 连接是否有效
     */
    bool IsValid() const;
    
    /*
     * @brief 重连到MySQL服务器
     * @return 是否成功重连
     */
    bool Reconnect();
    
    /*
     * @brief 获取原生的MySQL连接句柄
     * @return MySQL连接句柄
     */
    MYSQL* GetNativeHandle() { return mysql_; }
    
private:
    MYSQL* mysql_;          // MySQL连接句柄
    MySQLConfig config_;    // MySQL配置
    
    // 禁用拷贝
    MySQLConnection(const MySQLConnection&) = delete;
    MySQLConnection& operator=(const MySQLConnection&) = delete;
};

/*
 * @class MySQLConnectionPool
 * @brief MySQL连接池，管理多个MySQL连接
 */
class MySQLConnectionPool {
public:
    /*
     * @brief 构造函数
     * @param config MySQL配置
     */
    explicit MySQLConnectionPool(const MySQLConfig& config);
    
    /*
     * @brief 析构函数
     */
    ~MySQLConnectionPool();
    
    /*
     * @brief 获取一个可用的MySQL连接
     * @return MySQL连接（智能指针）
     */
    std::shared_ptr<MySQLConnection> GetConnection();
    
    /*
     * @brief 设置连接池大小
     * @param size 新的连接池大小
     */
    void SetPoolSize(int size);
    
    /**
     * @brief 获取连接池大小
     * @return 连接池大小
     */
    int GetPoolSize() const;
    
    /*
     * @brief 获取活跃连接数
     * @return 活跃连接数
     */
    // int GetActiveConnections() const;
    
    /*
     * @brief 检查是否有可用连接
     * @return 是否有可用连接
     */
    // bool HasAvailableConnections() const;
    
private:
    MySQLConfig config_;                                    // MySQL配置
    std::vector<std::shared_ptr<MySQLConnection>> pool_;    // 连接池
    mutable std::mutex mutex_;                              // 互斥锁
    
    /*
     * @brief 创建新的MySQL连接
     * @return 新的MySQL连接
     */
    std::shared_ptr<MySQLConnection> CreateConnection();
    
    /*
     * @brief 清理无效连接
     */
    // void CleanupInvalidConnections();
};

/*
 * @class MySQLStorage
 * @brief MySQL存储实现，用于持久化日志数据
 * 
 * 该类提供了对MySQL数据库的封装，包括表的创建、查询、插入、更新和删除等操作。
 * 主要用于日志数据的持久化存储。
 */
class MySQLStorage {
public:
    /*
     * @struct LogEntry
     * @brief 日志条目结构
     */
    struct LogEntry {
        std::string id;                 // 日志ID
        std::string timestamp;          // 时间戳
        std::string level;              // 日志级别
        std::string source;             // 日志来源
        std::string message;            // 日志消息
        std::unordered_map<std::string, std::string> fields;  // 自定义字段
    };
    
    /*
     * @brief 构造函数
     * @param config MySQL配置
     */
    explicit MySQLStorage(const MySQLConfig& config = MySQLConfig());
    
    /*
     * @brief 析构函数
     */
    ~MySQLStorage();
    
    /*
     * @brief 初始化数据库表结构
     * @return 初始化是否成功
     */
    bool Initialize();
    
    /*
     * @brief 保存日志条目
     * @param entry 日志条目
     * @return 操作是否成功
     */
    bool SaveLogEntry(const LogEntry& entry);
    
    /*
     * @brief 批量保存日志条目
     * @param entries 日志条目列表
     * @return 成功保存的条目数量
     */
    int SaveLogEntries(const std::vector<LogEntry>& entries);
    
    /*
     * @brief 根据条件查询日志条目
     * @param conditions 查询条件（字段名->值）
     * @param limit 最大返回数量，0表示不限
     * @param offset 起始偏移，0表示从头开始
     * @return 日志条目列表
     */
    std::vector<LogEntry> QueryLogEntries(
        const std::unordered_map<std::string, std::string>& conditions,
        int limit = 0,
        int offset = 0
    );
    
    /*
     * @brief 根据ID获取日志条目
     * @param id 日志ID
     * @return 日志条目，如果不存在则返回空条目
     */
    LogEntry GetLogEntryById(const std::string& id);
    
    /*
     * @brief 根据时间范围查询日志条目
     * @param startTime 开始时间（ISO 8601格式）
     * @param endTime 结束时间（ISO 8601格式）
     * @param limit 最大返回数量，0表示不限
     * @param offset 起始偏移，0表示从头开始
     * @return 日志条目列表
     */
    std::vector<LogEntry> QueryLogEntriesByTimeRange(
        const std::string& startTime,
        const std::string& endTime,
        int limit = 0,
        int offset = 0
    );
    
    /*
     * @brief 根据日志级别查询日志条目
     * @param level 日志级别
     * @param limit 最大返回数量，0表示不限
     * @param offset 起始偏移，0表示从头开始
     * @return 日志条目列表
     */
    std::vector<LogEntry> QueryLogEntriesByLevel(
        const std::string& level,
        int limit = 0,
        int offset = 0
    );
    
    /*
     * @brief 根据日志来源查询日志条目
     * @param source 日志来源
     * @param limit 最大返回数量，0表示不限
     * @param offset 起始偏移，0表示从头开始
     * @return 日志条目列表
     */
    std::vector<LogEntry> QueryLogEntriesBySource(
        const std::string& source,
        int limit = 0,
        int offset = 0
    );
    
    /*
     * @brief 根据消息内容全文搜索日志条目
     * @param keyword 关键词
     * @param limit 最大返回数量，0表示不限
     * @param offset 起始偏移，0表示从头开始
     * @return 日志条目列表
     */
    std::vector<LogEntry> SearchLogEntriesByKeyword(
        const std::string& keyword,
        int limit = 0,
        int offset = 0
    );
    
    /*
     * @brief 获取日志条目总数
     * @return 日志条目总数
     */
    int GetLogEntryCount();
    
    /*
     * @brief 删除过期日志条目
     * @param beforeTime 删除该时间之前的日志（ISO 8601格式 2023-11-15T00:00:00Z）
     * @return 删除的条目数量
     */
    int DeleteLogEntriesBefore(const std::string& beforeTime);
    
    /*
     * @brief 测试数据库连接
     * @return 连接是否正常
     */
    bool TestConnection();
    
private:
    std::unique_ptr<MySQLConnectionPool> pool_;  // MySQL连接池
    
    /*
     * @brief 将结果集转换为日志条目
     * @param row 结果集行
     * @return 日志条目
     */
    LogEntry RowToLogEntry(const std::unordered_map<std::string, std::string>& row);
    
    /*
     * @brief 构建查询条件SQL
     * @param conditions 查询条件
     * @return SQL条件子句
     */
    std::string BuildWhereClause(const std::unordered_map<std::string, std::string>& conditions);
    
    /*
     * @brief 获取日志条目的自定义字段
     * @param logId 日志ID
     * @return 自定义字段映射
     */
    std::unordered_map<std::string, std::string> GetLogEntryFields(const std::string& logId);
};

} // namespace storage
} // namespace xumj

#endif // XUMJ_STORAGE_MYSQL_STORAGE_H 