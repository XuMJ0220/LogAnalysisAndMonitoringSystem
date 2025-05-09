#include "xumj/storage/storage_factory.h"
#include <iostream>
#include <stdexcept>
#include <nlohmann/json.hpp>

namespace xumj {
namespace storage {

// 使用nlohmann json库简化解析
using json = nlohmann::json;

RedisConfig StorageFactory::CreateRedisConfigFromJson(const std::string& configJson) {
    RedisConfig config;
    
    try {
        json j = json::parse(configJson);
        
        if (j.contains("host") && j["host"].is_string()) {
            config.host = j["host"].get<std::string>();
        }
        
        if (j.contains("port") && j["port"].is_number()) {
            config.port = j["port"].get<int>();
        }
        
        if (j.contains("password") && j["password"].is_string()) {
            config.password = j["password"].get<std::string>();
        }
        
        if (j.contains("database") && j["database"].is_number()) {
            config.database = j["database"].get<int>();
        }
        
        if (j.contains("timeout") && j["timeout"].is_number()) {
            config.timeout = j["timeout"].get<int>();
        }
        
        if (j.contains("poolSize") && j["poolSize"].is_number()) {
            config.poolSize = j["poolSize"].get<int>();
        }
    } catch (const std::exception& e) {
        std::cerr << "Redis配置解析错误: " << e.what() << std::endl;
        // 使用默认配置
    }
    
    return config;
}

MySQLConfig StorageFactory::CreateMySQLConfigFromJson(const std::string& configJson) {
    MySQLConfig config;
    
    try {
        json j = json::parse(configJson);
        
        if (j.contains("host") && j["host"].is_string()) {
            config.host = j["host"].get<std::string>();
        }
        
        if (j.contains("port") && j["port"].is_number()) {
            config.port = j["port"].get<int>();
        }
        
        if (j.contains("username") && j["username"].is_string()) {
            config.username = j["username"].get<std::string>();
        }
        
        if (j.contains("password") && j["password"].is_string()) {
            config.password = j["password"].get<std::string>();
        }
        
        if (j.contains("database") && j["database"].is_string()) {
            config.database = j["database"].get<std::string>();
        }
        
        if (j.contains("timeout") && j["timeout"].is_number()) {
            config.timeout = j["timeout"].get<int>();
        }
        
        if (j.contains("poolSize") && j["poolSize"].is_number()) {
            config.poolSize = j["poolSize"].get<int>();
        }
    } catch (const std::exception& e) {
        std::cerr << "MySQL配置解析错误: " << e.what() << std::endl;
        // 使用默认配置
    }
    
    return config;
}

std::shared_ptr<void> StorageFactory::CreateStorage(StorageType type, const std::string& config) {
    switch (type) {
        case StorageType::REDIS: {
            RedisConfig redisConfig = CreateRedisConfigFromJson(config);
            return std::static_pointer_cast<void>(CreateRedisStorage(redisConfig));
        }
        case StorageType::MYSQL: {
            MySQLConfig mysqlConfig = CreateMySQLConfigFromJson(config);
            return std::static_pointer_cast<void>(CreateMySQLStorage(mysqlConfig));
        }
        default:
            throw std::invalid_argument("未知的存储类型");
    }
}

} // namespace storage
} // namespace xumj 