#ifndef XUMJ_STORAGE_REDIS_STORAGE_H
#define XUMJ_STORAGE_REDIS_STORAGE_H

#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <functional>
#include <unordered_map>
#include <hiredis/hiredis.h>

namespace xumj {
namespace storage {

/*
 * @class RedisStorageException
 * @brief Redis存储异常类
 */
class RedisStorageException : public std::runtime_error {
public:
    explicit RedisStorageException(const std::string& message)
        : std::runtime_error(message) {}
};

/*
 * @struct RedisConfig
 * @brief Redis连接配置
 */
struct RedisConfig {
    std::string host{"127.0.0.1"};  // Redis服务器主机名或IP
    int port{6379};                // Redis服务器端口
    std::string password{};        // Redis认证密码（可选）
    int database{0};               // Redis数据库索引
    int timeout{5000};             // 连接超时（毫秒）
    int poolSize{5};               // 连接池大小
};

/*
 * @class RedisConnection
 * @brief Redis连接封装
 */
class RedisConnection {
public:
    /*
     * @brief 构造函数
     * @param config Redis配置
     */
    explicit RedisConnection(const RedisConfig& config);
    
    /*
     * @brief 析构函数
     */
    ~RedisConnection();
    
    /*
     * @brief 执行Redis命令
     * @param format 命令格式
     * @param ... 可变参数
     * @return Redis响应
     */
    redisReply* Execute(const char* format, ...);
    
    /*
     * @brief 执行Redis命令（带参数数组）
     * @param argc 参数数量
     * @param argv 参数数组
     * @param argvlen 每个参数的长度数组
     * @return Redis响应
     */
    redisReply* ExecuteArgv(int argc, const char** argv, const size_t* argvlen);
    
    /*
     * @brief 检查连接是否有效
     * @return 连接是否有效
     */
    bool IsValid() const;
    
    /*
     * @brief 重连到Redis服务器
     * @return 是否成功重连
     */
    bool Reconnect();
    
private:
    redisContext* context_;  // Redis上下文
    RedisConfig config_;     // Redis配置
    
    // 禁用拷贝
    RedisConnection(const RedisConnection&) = delete;
    RedisConnection& operator=(const RedisConnection&) = delete;
};

/*
 * @class RedisConnectionPool
 * @brief Redis连接池，管理多个Redis连接
 */
class RedisConnectionPool {
public:
    /*
     * @brief 构造函数
     * @param config Redis配置
     */
    explicit RedisConnectionPool(const RedisConfig& config);
    
    /*
     * @brief 析构函数
     */
    ~RedisConnectionPool();
    
    /*
     * @brief 获取一个可用的Redis连接
     * @return Redis连接（智能指针）
     */
    std::shared_ptr<RedisConnection> GetConnection();
    
    /*
     * @brief 设置连接池大小
     * @param size 新的连接池大小
     */
    void SetPoolSize(int size);
    
    /*
     * @brief 获取连接池大小
     * @return 连接池大小
     */
    int GetPoolSize() const;
    
private:
    RedisConfig config_;                                    // Redis配置
    std::vector<std::shared_ptr<RedisConnection>> pool_;    // 连接池
    mutable std::mutex mutex_;                              // 互斥锁
    
    /*
     * @brief 创建新的Redis连接
     * @return 新的Redis连接
     */
    std::shared_ptr<RedisConnection> CreateConnection();
};

/*
 * @class RedisStorage
 * @brief Redis存储实现，用于高速缓存日志数据
 * 
 * 该类提供了对Redis数据存储的封装，包括多种数据类型（字符串、列表、散列表等）
 * 的操作，主要用于日志数据的高速缓存和临时存储。
 */
class RedisStorage {
public:
    /*
     * @brief 构造函数
     * @param config Redis配置
     */
    explicit RedisStorage(const RedisConfig& config = RedisConfig());
    
    /*
     * @brief 析构函数
     */
    ~RedisStorage();
    
    //====== 字符串操作 ======
    
    /*
     * @brief 设置键值
     * @param key 键
     * @param value 值
     * @param expireSeconds 过期时间（秒），0表示不过期
     * @return 操作是否成功
     */
    bool Set(const std::string& key, const std::string& value, int expireSeconds = 0);
    
    /*
     * @brief 获取键值
     * @param key 键
     * @param defaultValue 默认值（键不存在时返回）
     * @return 值
     */
    std::string Get(const std::string& key, const std::string& defaultValue = "");
    
    /*
     * @brief 删除键
     * @param key 键
     * @return 操作是否成功
     */
    bool Delete(const std::string& key);
    
    /*
     * @brief 检查键是否存在
     * @param key 键
     * @return 键是否存在
     */
    bool Exists(const std::string& key);
    
    /*
     * @brief 设置键的过期时间
     * @param key 键
     * @param expireSeconds 过期时间（秒）
     * @return 操作是否成功
     */
    bool Expire(const std::string& key, int expireSeconds);
    
    //====== 列表操作 ======
    
    /*
     * @brief 将元素添加到列表末尾
     * @param key 列表键
     * @param value 元素值
     * @return 添加后列表长度
     */
    int ListPush(const std::string& key, const std::string& value);
    
    /*
     * @brief 将元素添加到列表头部
     * @param key 列表键
     * @param value 元素值
     * @return 添加后列表长度
     */
    int ListPushFront(const std::string& key, const std::string& value);
    
    /*
     * @brief 从列表末尾弹出元素
     * @param key 列表键
     * @return 弹出的元素，如果列表为空则返回空字符串
     */
    std::string ListPop(const std::string& key);
    
    /*
     * @brief 从列表头部弹出元素
     * @param key 列表键
     * @return 弹出的元素，如果列表为空则返回空字符串
     */
    std::string ListPopFront(const std::string& key);
    
    /*
     * @brief 获取列表长度
     * @param key 列表键
     * @return 列表长度
     */
    int ListLength(const std::string& key);
    
    /*
     * @brief 获取列表中的元素
     * @param key 列表键
     * @param start 起始索引
     * @param end 结束索引，-1表示最后一个元素
     * @return 元素列表
     */
    std::vector<std::string> ListRange(const std::string& key, int start, int end);
    
    //====== 散列表操作 ======
    
    /*
     * @brief 设置散列表字段
     * @param key 散列表键
     * @param field 字段名
     * @param value 字段值
     * @return 操作是否成功
     */
    bool HashSet(const std::string& key, const std::string& field, const std::string& value);
    
    /*
     * @brief 获取散列表字段
     * @param key 散列表键
     * @param field 字段名
     * @param defaultValue 默认值（字段不存在时返回）
     * @return 字段值
     */
    std::string HashGet(const std::string& key, const std::string& field, const std::string& defaultValue = "");
    
    /*
     * @brief 删除散列表字段
     * @param key 散列表键
     * @param field 字段名
     * @return 操作是否成功
     */
    bool HashDelete(const std::string& key, const std::string& field);
    
    /*
     * @brief 检查散列表字段是否存在
     * @param key 散列表键
     * @param field 字段名
     * @return 字段是否存在
     */
    bool HashExists(const std::string& key, const std::string& field);
    
    /*
     * @brief 获取散列表中的所有字段和值
     * @param key 散列表键
     * @return 字段和值的映射
     */
    std::unordered_map<std::string, std::string> HashGetAll(const std::string& key);
    
    //====== 集合操作 ======
    
    /*
     * @brief 向集合添加元素
     * @param key 集合键
     * @param member 元素
     * @return 实际添加的元素数量（0表示元素已存在）
     */
    int SetAdd(const std::string& key, const std::string& member);
    
    /*
     * @brief 从集合删除元素
     * @param key 集合键
     * @param member 元素
     * @return 实际删除的元素数量（0表示元素不存在）
     */
    int SetRemove(const std::string& key, const std::string& member);
    
    /*
     * @brief 检查元素是否是集合的成员
     * @param key 集合键
     * @param member 元素
     * @return 元素是否在集合中
     */
    bool SetIsMember(const std::string& key, const std::string& member);
    
    /*
     * @brief 获取集合中的所有元素
     * @param key 集合键
     * @return 元素列表
     */
    std::vector<std::string> SetMembers(const std::string& key);
    
    /*
     * @brief 获取集合元素数量
     * @param key 集合键
     * @return 元素数量
     */
    int SetSize(const std::string& key);
    
    //====== 事务操作 ======
    
    /*
     * @brief 开始事务
     * @return 操作是否成功
     */
    bool Multi();
    
    /*
     * @brief 执行事务
     * @return 执行结果列表
     */
    std::vector<std::string> Exec();
    
    /*
     * @brief 放弃事务
     * @return 操作是否成功
     */
    bool Discard();
    
    //====== 连接管理 ======
    
    /*
     * @brief 测试连接是否可用
     * @return 连接是否可用
     */
    bool Ping();
    
    /*
     * @brief 获取Redis服务器信息
     * @return 服务器信息字符串
     */
    std::string Info();
    
private:
    std::unique_ptr<RedisConnectionPool> pool_;  // Redis连接池
    std::shared_ptr<RedisConnection> transactionConn_;  // 事务专用连接
    
    /*
     * @brief 执行Redis命令并获取回复
     * @param command 命令函数
     * @return 回复内容
     */
    std::string ExecuteCommand(std::function<redisReply*(RedisConnection*)> command);
};

} // namespace storage
} // namespace xumj

#endif // XUMJ_STORAGE_REDIS_STORAGE_H 