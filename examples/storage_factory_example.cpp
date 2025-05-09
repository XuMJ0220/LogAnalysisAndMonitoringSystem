#include <iostream>
#include <memory>
#include <string>
#include "xumj/storage/storage_factory.h"

using namespace xumj::storage;

// Redis配置JSON示例
const std::string REDIS_CONFIG_JSON = R"(
{
    "host": "127.0.0.1",
    "port": 6379,
    "password": "",
    "database": 0,
    "timeout": 5000,
    "poolSize": 3
}
)";

// MySQL配置JSON示例
const std::string MYSQL_CONFIG_JSON = R"(
{
    "host": "127.0.0.1",
    "port": 3306,
    "username": "root",
    "password": "password",
    "database": "log_system",
    "timeout": 5,
    "poolSize": 3
}
)";

void TestRedisFactory() {
    std::cout << "====== 测试Redis工厂 ======" << std::endl;
    
    try {
        // 从JSON创建Redis配置
        RedisConfig config = StorageFactory::CreateRedisConfigFromJson(REDIS_CONFIG_JSON);
        
        std::cout << "Redis配置:" << std::endl;
        std::cout << "  主机: " << config.host << std::endl;
        std::cout << "  端口: " << config.port << std::endl;
        std::cout << "  数据库: " << config.database << std::endl;
        std::cout << "  超时: " << config.timeout << "ms" << std::endl;
        std::cout << "  连接池大小: " << config.poolSize << std::endl;
        
        // 使用工厂创建Redis存储
        std::shared_ptr<RedisStorage> redis;
        try {
            redis = StorageFactory::CreateRedisStorage(config);
            
            // 测试连接
            bool connected = redis->Ping();
            std::cout << "Redis连接状态: " << (connected ? "已连接" : "未连接") << std::endl;
            
            if (connected) {
                // 简单的操作测试
                redis->Set("factory_test", "通过工厂类创建的Redis存储");
                std::string value = redis->Get("factory_test");
                std::cout << "存储的值: " << value << std::endl;
                
                // 清理
                redis->Delete("factory_test");
            }
        } catch (const RedisStorageException& e) {
            std::cerr << "Redis错误: " << e.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "Redis配置错误: " << e.what() << std::endl;
    }
}

void TestMySQLFactory() {
    std::cout << "\n====== 测试MySQL工厂 ======" << std::endl;
    
    try {
        // 从JSON创建MySQL配置
        MySQLConfig config = StorageFactory::CreateMySQLConfigFromJson(MYSQL_CONFIG_JSON);
        
        std::cout << "MySQL配置:" << std::endl;
        std::cout << "  主机: " << config.host << std::endl;
        std::cout << "  端口: " << config.port << std::endl;
        std::cout << "  用户名: " << config.username << std::endl;
        std::cout << "  数据库: " << config.database << std::endl;
        std::cout << "  超时: " << config.timeout << "s" << std::endl;
        std::cout << "  连接池大小: " << config.poolSize << std::endl;
        
        // 使用工厂创建MySQL存储
        std::shared_ptr<MySQLStorage> mysql;
        try {
            mysql = StorageFactory::CreateMySQLStorage(config);
            
            // 测试连接
            bool connected = mysql->TestConnection();
            std::cout << "MySQL连接状态: " << (connected ? "已连接" : "未连接") << std::endl;
            
            if (connected) {
                // 初始化表结构
                bool initialized = mysql->Initialize();
                std::cout << "表结构初始化: " << (initialized ? "成功" : "失败") << std::endl;
                
                if (initialized) {
                    // 简单的操作测试
                    MySQLStorage::LogEntry entry;
                    entry.timestamp = "2023-07-15 14:30:00";
                    entry.level = "INFO";
                    entry.source = "工厂示例";
                    entry.message = "通过工厂类创建的MySQL存储";
                    entry.fields["test"] = "factory";
                    
                    bool saved = mysql->SaveLogEntry(entry);
                    std::cout << "日志条目保存: " << (saved ? "成功" : "失败") << std::endl;
                    
                    // 查询日志条数
                    int count = mysql->GetLogEntryCount();
                    std::cout << "总日志条数: " << count << std::endl;
                }
            }
        } catch (const MySQLStorageException& e) {
            std::cerr << "MySQL错误: " << e.what() << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "MySQL配置错误: " << e.what() << std::endl;
    }
}

void TestGenericFactory() {
    std::cout << "\n====== 测试通用工厂 ======" << std::endl;
    
    // 创建Redis存储
    std::cout << "通过通用工厂创建Redis存储..." << std::endl;
    try {
        std::shared_ptr<void> redisVoid = StorageFactory::CreateStorage(StorageType::REDIS, REDIS_CONFIG_JSON);
        auto redis = std::static_pointer_cast<RedisStorage>(redisVoid);
        
        bool redisConnected = redis->Ping();
        std::cout << "Redis连接状态: " << (redisConnected ? "已连接" : "未连接") << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Redis错误: " << e.what() << std::endl;
    }
    
    // 创建MySQL存储
    std::cout << "\n通过通用工厂创建MySQL存储..." << std::endl;
    try {
        std::shared_ptr<void> mysqlVoid = StorageFactory::CreateStorage(StorageType::MYSQL, MYSQL_CONFIG_JSON);
        auto mysql = std::static_pointer_cast<MySQLStorage>(mysqlVoid);
        
        bool mysqlConnected = mysql->TestConnection();
        std::cout << "MySQL连接状态: " << (mysqlConnected ? "已连接" : "未连接") << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "MySQL错误: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "开始存储工厂测试程序..." << std::endl;
    
    // 测试Redis工厂
    TestRedisFactory();
    
    // 测试MySQL工厂
    TestMySQLFactory();
    
    // 测试通用工厂
    TestGenericFactory();
    
    std::cout << "\n存储工厂测试程序结束！" << std::endl;
    return 0;
} 