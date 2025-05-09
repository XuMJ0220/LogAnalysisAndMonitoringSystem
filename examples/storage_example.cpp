#include <iostream>
#include <vector>
#include <string>
#include <chrono>
#include <thread>
#include <iomanip>
#include <ctime>
#include "xumj/storage/redis_storage.h"
#include "xumj/storage/mysql_storage.h"

using namespace xumj::storage;

// 获取当前时间的ISO 8601格式字符串
std::string GetCurrentTimeISO8601() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now = *std::localtime(&time_t_now);
    
    char buffer[30];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &tm_now);
    
    return std::string(buffer);
}

// 测试Redis存储功能
void TestRedisStorage() {
    std::cout << "====== 测试Redis存储 ======" << std::endl;
    
    // 创建Redis配置
    RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.password = "";
    config.database = 0;
    
    try {
        // 创建Redis存储
        RedisStorage redis(config);
        
        // 测试基本连接
        if (redis.Ping()) {
            std::cout << "Redis连接成功！" << std::endl;
        } else {
            std::cout << "Redis连接失败！" << std::endl;
            return;
        }
        
        // 测试字符串操作
        std::cout << "\n-- 字符串操作 --" << std::endl;
        redis.Set("test_key", "测试值", 60);
        std::string value = redis.Get("test_key");
        std::cout << "test_key = " << value << std::endl;
        
        // 测试列表操作
        std::cout << "\n-- 列表操作 --" << std::endl;
        redis.Delete("test_list");
        redis.ListPush("test_list", "列表项1");
        redis.ListPush("test_list", "列表项2");
        redis.ListPush("test_list", "列表项3");
        int listLength = redis.ListLength("test_list");
        std::cout << "列表长度: " << listLength << std::endl;
        
        auto listItems = redis.ListRange("test_list", 0, -1);
        std::cout << "列表内容:" << std::endl;
        for (const auto& item : listItems) {
            std::cout << "  - " << item << std::endl;
        }
        
        // 测试散列表操作
        std::cout << "\n-- 散列表操作 --" << std::endl;
        redis.Delete("test_hash");
        redis.HashSet("test_hash", "field1", "值1");
        redis.HashSet("test_hash", "field2", "值2");
        redis.HashSet("test_hash", "field3", "值3");
        
        std::cout << "散列表字段 field1 = " << redis.HashGet("test_hash", "field1") << std::endl;
        std::cout << "散列表字段 field2 = " << redis.HashGet("test_hash", "field2") << std::endl;
        std::cout << "散列表字段 field3 = " << redis.HashGet("test_hash", "field3") << std::endl;
        
        auto hashFields = redis.HashGetAll("test_hash");
        std::cout << "散列表所有字段:" << std::endl;
        for (const auto& field : hashFields) {
            std::cout << "  - " << field.first << ": " << field.second << std::endl;
        }
        
        // 测试集合操作
        std::cout << "\n-- 集合操作 --" << std::endl;
        redis.Delete("test_set");
        redis.SetAdd("test_set", "成员1");
        redis.SetAdd("test_set", "成员2");
        redis.SetAdd("test_set", "成员3");
        
        std::cout << "集合大小: " << redis.SetSize("test_set") << std::endl;
        std::cout << "成员1是否在集合中: " << (redis.SetIsMember("test_set", "成员1") ? "是" : "否") << std::endl;
        std::cout << "成员4是否在集合中: " << (redis.SetIsMember("test_set", "成员4") ? "是" : "否") << std::endl;
        
        auto setMembers = redis.SetMembers("test_set");
        std::cout << "集合成员:" << std::endl;
        for (const auto& member : setMembers) {
            std::cout << "  - " << member << std::endl;
        }
        
        // 测试过期时间
        std::cout << "\n-- 过期时间操作 --" << std::endl;
        redis.Set("expire_key", "这个键将在5秒后过期", 5);
        std::cout << "expire_key = " << redis.Get("expire_key") << std::endl;
        std::cout << "等待5秒..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
        std::cout << "5秒后，expire_key 是否存在: " << (redis.Exists("expire_key") ? "是" : "否") << std::endl;
        
        // 清理
        redis.Delete("test_key");
        redis.Delete("test_list");
        redis.Delete("test_hash");
        redis.Delete("test_set");
        
        std::cout << "\nRedis存储测试完成！" << std::endl;
    } catch (const RedisStorageException& e) {
        std::cerr << "Redis错误: " << e.what() << std::endl;
    }
}

// 测试MySQL存储功能
void TestMySQLStorage() {
    std::cout << "\n====== 测试MySQL存储 ======" << std::endl;
    
    // 创建MySQL配置
    MySQLConfig config;
    config.host = "127.0.0.1";
    config.port = 3306;
    config.username = "root";
    config.password = "ytfhqqkso1";  // 使用正确的密码
    config.database = "mydatabase1";  // 使用已存在的数据库
    
    try {
        // 创建MySQL存储
        MySQLStorage mysql(config);
        
        // 测试连接
        if (mysql.TestConnection()) {
            std::cout << "MySQL连接成功！" << std::endl;
        } else {
            std::cout << "MySQL连接失败！" << std::endl;
            return;
        }
        
        // 初始化数据库表结构
        if (mysql.Initialize()) {
            std::cout << "数据库表结构初始化成功！" << std::endl;
        } else {
            std::cout << "数据库表结构初始化失败！" << std::endl;
            return;
        }
        
        // 测试保存日志条目
        std::cout << "\n-- 保存日志条目 --" << std::endl;
        
        MySQLStorage::LogEntry entry;
        entry.timestamp = GetCurrentTimeISO8601();
        entry.level = "INFO";
        entry.source = "存储示例";
        entry.message = "这是一条测试日志消息";
        entry.fields["user"] = "admin";
        entry.fields["ip"] = "192.168.1.1";
        entry.fields["module"] = "存储模块";
        
        if (mysql.SaveLogEntry(entry)) {
            std::cout << "日志条目保存成功！" << std::endl;
        } else {
            std::cout << "日志条目保存失败！" << std::endl;
        }
        
        // 测试批量保存日志条目
        std::cout << "\n-- 批量保存日志条目 --" << std::endl;
        
        std::vector<MySQLStorage::LogEntry> entries;
        for (int i = 0; i < 5; ++i) {
            MySQLStorage::LogEntry e;
            e.timestamp = GetCurrentTimeISO8601();
            e.level = (i % 3 == 0) ? "ERROR" : ((i % 2 == 0) ? "WARNING" : "INFO");
            e.source = "存储示例";
            e.message = "这是批量测试消息 #" + std::to_string(i + 1);
            e.fields["batch"] = "test";
            e.fields["index"] = std::to_string(i);
            entries.push_back(e);
            
            // 等待1秒，使时间戳不同
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        int saved = mysql.SaveLogEntries(entries);
        std::cout << "成功保存 " << saved << " 条日志条目" << std::endl;
        
        // 测试查询日志条目
        std::cout << "\n-- 查询日志条目 --" << std::endl;
        
        std::cout << "总日志条目数: " << mysql.GetLogEntryCount() << std::endl;
        
        // 按级别查询
        std::cout << "\n按级别查询 (ERROR):" << std::endl;
        auto errorLogs = mysql.QueryLogEntriesByLevel("ERROR", 10, 0);
        for (const auto& log : errorLogs) {
            std::cout << "  - [" << log.timestamp << "] " << log.level << " [" << log.source << "]: " << log.message << std::endl;
            std::cout << "    自定义字段:" << std::endl;
            for (const auto& field : log.fields) {
                std::cout << "      " << field.first << ": " << field.second << std::endl;
            }
        }
        
        // 按来源查询
        std::cout << "\n按来源查询 (存储示例):" << std::endl;
        auto sourceLogs = mysql.QueryLogEntriesBySource("存储示例", 3, 0);
        for (const auto& log : sourceLogs) {
            std::cout << "  - [" << log.timestamp << "] " << log.level << ": " << log.message << std::endl;
        }
        
        // 关键字搜索
        std::cout << "\n关键字搜索 (批量):" << std::endl;
        auto keywordLogs = mysql.SearchLogEntriesByKeyword("批量", 10, 0);
        for (const auto& log : keywordLogs) {
            std::cout << "  - [" << log.timestamp << "] " << log.level << ": " << log.message << std::endl;
        }
        
        std::cout << "\nMySQL存储测试完成！" << std::endl;
    } catch (const MySQLStorageException& e) {
        std::cerr << "MySQL错误: " << e.what() << std::endl;
    }
}

int main() {
    try {
        std::cout << "开始存储模块测试程序..." << std::endl;
        
        // 测试Redis存储
        TestRedisStorage();
        
        // 测试MySQL存储
        TestMySQLStorage();
        
        std::cout << "\n存储模块测试程序结束！" << std::endl;
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "发生错误: " << e.what() << std::endl;
        return 1;
    }
} 