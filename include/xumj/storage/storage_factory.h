#ifndef XUMJ_STORAGE_FACTORY_H
#define XUMJ_STORAGE_FACTORY_H

#include <memory>
#include <string>
#include <unordered_map>
#include "xumj/storage/redis_storage.h"
#include "xumj/storage/mysql_storage.h"

namespace xumj {
namespace storage {

/*
 * @enum StorageType
 * @brief 存储类型枚举
 */
enum class StorageType {
    REDIS,  // Redis存储
    MYSQL   // MySQL存储
};

/*
 * @class StorageFactory
 * @brief 存储工厂类，用于创建不同类型的存储实例
 */
class StorageFactory {
public:
    /*
     * @brief 构造函数
     */
    StorageFactory() = default;

    /*
     * @brief 注册存储实例
     * @param name 存储名称
     * @param storage 存储实例
     * @return 是否成功注册
     */
    template<typename T>
    bool RegisterStorage(const std::string& name, std::shared_ptr<T> storage) {
        if (storages_.find(name) != storages_.end()) {
            return false; // 已存在同名存储
        }
        storages_[name] = std::static_pointer_cast<void>(storage);
        return true;
    }

    /*
     * @brief 获取存储实例
     * @param name 存储名称
     * @return 存储实例（需要转换为具体类型）
     */
    template<typename T>
    std::shared_ptr<T> GetStorage(const std::string& name) {
        auto it = storages_.find(name);
        if (it == storages_.end()) {
            return nullptr;
        }
        return std::static_pointer_cast<T>(it->second);
    }

    /*
     * @brief 创建Redis存储实例
     * @param config Redis配置
     * @return Redis存储实例
     */
    static std::shared_ptr<RedisStorage> CreateRedisStorage(const RedisConfig& config = RedisConfig()) {
        return std::make_shared<RedisStorage>(config);
    }
    
    /*
     * @brief 创建MySQL存储实例
     * @param config MySQL配置
     * @return MySQL存储实例
     */
    static std::shared_ptr<MySQLStorage> CreateMySQLStorage(const MySQLConfig& config = MySQLConfig()) {
        return std::make_shared<MySQLStorage>(config);
    }
    
    /*
     * @brief 根据存储类型创建相应的存储实例
     * @param type 存储类型
     * @param config 配置JSON字符串
     * @return 存储实例（需要转换为具体类型）
     */
    static std::shared_ptr<void> CreateStorage(StorageType type, const std::string& config = "{}");
    
    /*
     * @brief 从JSON字符串创建Redis配置
     * @param configJson 配置JSON字符串
     * @return Redis配置
     */
    static RedisConfig CreateRedisConfigFromJson(const std::string& configJson);
    
    /*
     * @brief 从JSON字符串创建MySQL配置
     * @param configJson 配置JSON字符串
     * @return MySQL配置
     */
    static MySQLConfig CreateMySQLConfigFromJson(const std::string& configJson);

private:
    std::unordered_map<std::string, std::shared_ptr<void>> storages_; // 存储实例映射
};

} // namespace storage
} // namespace xumj

#endif // XUMJ_STORAGE_FACTORY_H 