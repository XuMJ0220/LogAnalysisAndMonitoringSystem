#include <xumj/storage/storage_factory.h>
#include <iostream>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <stdexcept>
#include <nlohmann/json.hpp>

using namespace xumj::storage;
using json = nlohmann::json;

// 工具函数：打印分隔线
void printSeparator() {
    std::cout << "\n" << std::string(60, '-') << "\n" << std::endl;
}

// 工具函数：获取当前时间戳
std::string getCurrentTimestamp() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&now_c);
    
    char buffer[80];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm);
    return std::string(buffer);
}

int main(int argc, char** argv) {
    // 标记参数已使用，避免编译警告
    (void)argc;
    (void)argv;
    
    try {
        std::cout << "存储工厂示例程序启动...\n" << std::endl;
        
        // 创建一个存储工厂实例
        StorageFactory factory;
        
        printSeparator();
        std::cout << "1. 从JSON配置创建Redis和MySQL存储" << std::endl;
        
        // 创建Redis配置JSON
        json redisConfig;
        redisConfig["host"] = "127.0.0.1";
        redisConfig["port"] = 6379;
        redisConfig["password"] = "123465";  // 使用Redis配置文件中的密码
        redisConfig["database"] = 0;
        redisConfig["timeout"] = 3000;
        redisConfig["poolSize"] = 2;
        
        // 创建MySQL配置JSON
        json mysqlConfig;
        mysqlConfig["host"] = "127.0.0.1";
        mysqlConfig["port"] = 3306;
        mysqlConfig["username"] = "root";
        mysqlConfig["password"] = "ytfhqqkso1";
        mysqlConfig["database"] = "xumj_logs_test";
        mysqlConfig["timeout"] = 5;
        mysqlConfig["poolSize"] = 2;
        
        // 使用工厂从JSON创建配置对象
        std::cout << "Redis配置JSON: " << redisConfig.dump(2) << std::endl;
        RedisConfig redisCfg = StorageFactory::CreateRedisConfigFromJson(redisConfig.dump());
        
        std::cout << "MySQL配置JSON: " << mysqlConfig.dump(2) << std::endl;
        MySQLConfig mysqlCfg = StorageFactory::CreateMySQLConfigFromJson(mysqlConfig.dump());
        
        // 显示解析后的配置
        std::cout << "\n解析后的Redis配置:" << std::endl;
        std::cout << "  Host: " << redisCfg.host << std::endl;
        std::cout << "  Port: " << redisCfg.port << std::endl;
        std::cout << "  Database: " << redisCfg.database << std::endl;
        std::cout << "  Timeout: " << redisCfg.timeout << "ms" << std::endl;
        std::cout << "  Pool Size: " << redisCfg.poolSize << std::endl;
        
        std::cout << "\n解析后的MySQL配置:" << std::endl;
        std::cout << "  Host: " << mysqlCfg.host << std::endl;
        std::cout << "  Port: " << mysqlCfg.port << std::endl;
        std::cout << "  Username: " << mysqlCfg.username << std::endl;
        std::cout << "  Database: " << mysqlCfg.database << std::endl;
        std::cout << "  Timeout: " << mysqlCfg.timeout << "s" << std::endl;
        std::cout << "  Pool Size: " << mysqlCfg.poolSize << std::endl;
        
        printSeparator();
        std::cout << "2. 使用工厂静态方法创建存储实例" << std::endl;
        
        // 使用工厂静态方法创建Redis和MySQL存储实例
        auto redisStorage = StorageFactory::CreateRedisStorage(redisCfg);
        auto mysqlStorage = StorageFactory::CreateMySQLStorage(mysqlCfg);
        
        // 测试Redis连接
        bool redisConnected = false;
        try {
            redisConnected = redisStorage->Ping();
            std::cout << "Redis连接测试: " << (redisConnected ? "成功" : "失败") << std::endl;
        } catch (const std::exception& e) {
            std::cout << "Redis连接测试失败: " << e.what() << std::endl;
        }
        
        // 测试MySQL连接
        bool mysqlConnected = false;
        try {
            mysqlConnected = mysqlStorage->TestConnection();
            std::cout << "MySQL连接测试: " << (mysqlConnected ? "成功" : "失败") << std::endl;
            
            if (mysqlConnected) {
                // 初始化MySQL表结构
                mysqlStorage->Initialize();
            }
        } catch (const std::exception& e) {
            std::cout << "MySQL连接测试失败: " << e.what() << std::endl;
        }
        
        printSeparator();
        std::cout << "3. 使用StorageType枚举创建存储实例" << std::endl;
        
        // 使用StorageType枚举和JSON字符串创建存储实例
        std::cout << "通过StorageType创建Redis实例..." << std::endl;
        auto redisStorageAlt = std::static_pointer_cast<RedisStorage>(
            StorageFactory::CreateStorage(StorageType::REDIS, redisConfig.dump())
        );
        
        if (redisStorageAlt) {
            try {
                std::cout << "Redis ping测试: " << (redisStorageAlt->Ping() ? "成功" : "失败") << std::endl;
            } catch (const std::exception& e) {
                std::cout << "Redis ping测试异常: " << e.what() << std::endl;
            }
        }
        
        std::cout << "通过StorageType创建MySQL实例..." << std::endl;
        auto mysqlStorageAlt = std::static_pointer_cast<MySQLStorage>(
            StorageFactory::CreateStorage(StorageType::MYSQL, mysqlConfig.dump())
        );
        
        if (mysqlStorageAlt) {
            try {
                std::cout << "MySQL连接测试: " << (mysqlStorageAlt->TestConnection() ? "成功" : "失败") << std::endl;
            } catch (const std::exception& e) {
                std::cout << "MySQL连接测试异常: " << e.what() << std::endl;
            }
        }
        
        printSeparator();
        std::cout << "4. 向工厂注册和获取存储实例" << std::endl;
        
        // 向工厂注册存储实例
        if (redisConnected) {
            bool registered = factory.RegisterStorage("main-redis", redisStorage);
            std::cout << "向工厂注册Redis存储: " << (registered ? "成功" : "失败") << std::endl;
        }
        
        if (mysqlConnected) {
            bool registered = factory.RegisterStorage("main-mysql", mysqlStorage);
            std::cout << "向工厂注册MySQL存储: " << (registered ? "成功" : "失败") << std::endl;
        }
        
        // 从工厂获取存储实例
        auto retrievedRedis = factory.GetStorage<RedisStorage>("main-redis");
        if (retrievedRedis) {
            std::cout << "从工厂获取Redis存储: 成功" << std::endl;
            
            // 简单测试
            std::string testKey = "factory-test:redis";
            retrievedRedis->Set(testKey, "Storage factory test at " + getCurrentTimestamp());
            std::string value = retrievedRedis->Get(testKey);
            std::cout << "Redis测试结果: " << value << std::endl;
            retrievedRedis->Delete(testKey);
        } else {
            std::cout << "从工厂获取Redis存储: 失败" << std::endl;
        }
        
        auto retrievedMySQL = factory.GetStorage<MySQLStorage>("main-mysql");
        if (retrievedMySQL) {
            std::cout << "从工厂获取MySQL存储: 成功" << std::endl;
            
            // 简单测试
            MySQLStorage::LogEntry entry;
            entry.id = "factory-test-" + std::to_string(std::time(nullptr));
            entry.timestamp = getCurrentTimestamp();
            entry.level = "INFO";
            entry.source = "factory-example";
            entry.message = "Storage factory test";
            entry.fields["test_field"] = "test_value";
            
            bool saved = retrievedMySQL->SaveLogEntry(entry);
            std::cout << "MySQL测试结果 (保存日志): " << (saved ? "成功" : "失败") << std::endl;
            
            if (saved) {
                auto retrievedEntry = retrievedMySQL->GetLogEntryById(entry.id);
                std::cout << "MySQL测试结果 (获取日志): " << 
                    (retrievedEntry.id == entry.id ? "成功" : "失败") << std::endl;
            }
        } else {
            std::cout << "从工厂获取MySQL存储: 失败" << std::endl;
        }
        
        printSeparator();
        std::cout << "5. 测试多个存储实例的并发使用" << std::endl;
        
        if (retrievedRedis && retrievedMySQL) {
            // 创建第二个Redis实例
            RedisConfig redisCfg2 = redisCfg;
            redisCfg2.database = 1; // 使用不同的数据库
            auto redisStorage2 = StorageFactory::CreateRedisStorage(redisCfg2);
            factory.RegisterStorage("secondary-redis", redisStorage2);
            
            std::cout << "并发向不同存储写入数据..." << std::endl;
            
            // 测试键
            std::string redisKey1 = "concurrent-test:redis1";
            std::string redisKey2 = "concurrent-test:redis2";
            std::string mysqlLogId = "concurrent-test-" + std::to_string(std::time(nullptr));
            
            // 并发写入Redis和MySQL
            retrievedRedis->Set(redisKey1, "主Redis实例数据 " + getCurrentTimestamp());
            auto redis2 = factory.GetStorage<RedisStorage>("secondary-redis");
            if (redis2) {
                redis2->Set(redisKey2, "副Redis实例数据 " + getCurrentTimestamp());
            }
            
            MySQLStorage::LogEntry logEntry;
            logEntry.id = mysqlLogId;
            logEntry.timestamp = getCurrentTimestamp();
            logEntry.level = "INFO";
            logEntry.source = "concurrent-test";
            logEntry.message = "并发测试日志消息";
            retrievedMySQL->SaveLogEntry(logEntry);
            
            // 读取数据
            std::cout << "\n读取写入的数据:" << std::endl;
            std::cout << "主Redis: " << retrievedRedis->Get(redisKey1) << std::endl;
            if (redis2) {
                std::cout << "副Redis: " << redis2->Get(redisKey2) << std::endl;
            }
            
            auto mysqlEntry = retrievedMySQL->GetLogEntryById(mysqlLogId);
            if (!mysqlEntry.id.empty()) {
                std::cout << "MySQL: " << mysqlEntry.timestamp << " [" << 
                    mysqlEntry.level << "] " << mysqlEntry.message << std::endl;
            }
            
            // 清理测试数据
            retrievedRedis->Delete(redisKey1);
            if (redis2) {
                redis2->Delete(redisKey2);
            }
        } else {
            std::cout << "跳过并发测试 (存储实例不可用)" << std::endl;
        }
        
        printSeparator();
        std::cout << "存储工厂示例程序执行完毕。" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 