#include <iostream>
#include <string>
#include <ctime>
#include <chrono>
#include "xumj/storage/mysql_storage.h"

using namespace xumj::storage;

int main() {
    try {
        std::cout << "========== MySQL存储测试程序 ==========" << std::endl;
        
        // 创建MySQL配置
        MySQLConfig config;
        config.host = "127.0.0.1";
        config.port = 3306;
        config.username = "root";
        config.password = "ytfhqqkso1";
        config.database = "log_analysis";
        
        std::cout << "MySQL配置: " << config.host << ":" << config.port 
                  << ", 用户: " << config.username 
                  << ", 数据库: " << config.database << std::endl;

        // 创建MySQL存储实例
        std::cout << "正在创建MySQL存储实例..." << std::endl;
        MySQLStorage storage(config);
        std::cout << "MySQL存储创建成功" << std::endl;

        // 测试连接
        std::cout << "正在测试连接..." << std::endl;
        bool connSuccess = storage.TestConnection();
        std::cout << "测试连接结果: " << (connSuccess ? "成功" : "失败") << std::endl;

        if (!connSuccess) {
            std::cerr << "无法继续，数据库连接失败" << std::endl;
            return 1;
        }

        // 初始化表结构
        std::cout << "正在初始化表结构..." << std::endl;
        bool initSuccess = storage.Initialize();
        std::cout << "表结构初始化: " << (initSuccess ? "成功" : "失败") << std::endl;

        if (!initSuccess) {
            std::cerr << "无法继续，表结构初始化失败" << std::endl;
            return 1;
        }

        // 创建一个临时的MySQL连接查看表信息
        std::cout << "正在创建临时MySQL连接..." << std::endl;
        MySQLConnection conn(config);

        // 测试表是否存在
        std::cout << "正在测试表是否存在..." << std::endl;
        try {
            auto result = conn.Query("SHOW TABLES");
            std::cout << "数据库中的表: " << std::endl;
            for (const auto& row : result) {
                for (const auto& field : row) {
                    std::cout << "- " << field.first << ": " << field.second << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "查询表失败: " << e.what() << std::endl;
        }

        // 测试日志表结构
        std::cout << "正在测试日志表结构..." << std::endl;
        try {
            auto result = conn.Query("DESCRIBE log_entries");
            std::cout << "log_entries 表结构: " << std::endl;
            for (const auto& row : result) {
                std::cout << "- 字段: ";
                for (const auto& field : row) {
                    std::cout << field.first << "=" << field.second << " ";
                }
                std::cout << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "查询表结构失败: " << e.what() << std::endl;
        }

        // 创建日志条目
        std::string timestamp = std::to_string(std::time(nullptr));
        std::string id = "test-debug-" + timestamp;
        
        MySQLStorage::LogEntry entry;
        entry.id = id;
        entry.timestamp = "2025-05-11 01:10:00";
        entry.level = "DEBUG";
        entry.source = "test-source";
        entry.message = "测试消息";
        entry.fields["field1"] = "value1";
        entry.fields["field2"] = "value2";

        std::cout << "创建日志条目:" << std::endl;
        std::cout << "- ID: " << entry.id << std::endl;
        std::cout << "- 时间戳: " << entry.timestamp << std::endl;
        std::cout << "- 级别: " << entry.level << std::endl;
        std::cout << "- 来源: " << entry.source << std::endl;
        std::cout << "- 消息: " << entry.message << std::endl;
        std::cout << "- 字段数: " << entry.fields.size() << std::endl;

        // 尝试直接执行SQL插入
        std::cout << "正在尝试直接执行SQL插入..." << std::endl;
        bool directSuccess = false;
        try {
            std::string sql = "INSERT INTO log_entries (id, timestamp, level, source, message) VALUES ('" 
                           + conn.EscapeString(id) + "', '" 
                           + conn.EscapeString(entry.timestamp) + "', '" 
                           + conn.EscapeString(entry.level) + "', '" 
                           + conn.EscapeString(entry.source) + "', '" 
                           + conn.EscapeString(entry.message) + "')";
             
            std::cout << "执行SQL: " << sql << std::endl;
            conn.Execute(sql);
            directSuccess = true;
            std::cout << "直接SQL插入成功" << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "直接SQL插入失败: " << e.what() << std::endl;
        }

        // 删除刚插入的测试记录
        if (directSuccess) {
            try {
                std::string sql = "DELETE FROM log_entries WHERE id = '" + conn.EscapeString(id) + "'";
                conn.Execute(sql);
                std::cout << "删除测试记录成功" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "删除测试记录失败: " << e.what() << std::endl;
            }
        }

        // 保存日志条目
        std::cout << "正在通过SaveLogEntry保存日志条目，ID: " << entry.id << std::endl;
        bool saveSuccess = false;
        try {
            saveSuccess = storage.SaveLogEntry(entry);
            std::cout << "SaveLogEntry结果: " << (saveSuccess ? "成功" : "失败") << std::endl;
        } catch (const MySQLStorageException& e) {
            std::cerr << "SaveLogEntry出现MySQLStorageException: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "SaveLogEntry出现未知异常: " << e.what() << std::endl;
        }

        // 验证日志条目是否保存成功
        std::cout << "正在验证日志条目是否保存成功..." << std::endl;
        try {
            auto result = conn.Query("SELECT COUNT(*) as count FROM log_entries WHERE id = '" + conn.EscapeString(entry.id) + "'");
            if (!result.empty()) {
                auto& row = result[0];
                if (row.find("count") != row.end()) {
                    int count = std::stoi(row["count"]);
                    std::cout << "查询结果: 找到 " << count << " 条记录" << std::endl;
                } else {
                    std::cout << "查询结果: 没有找到记录" << std::endl;
                }
            } else {
                std::cout << "查询结果: 没有找到记录" << std::endl;
            }
        } catch (const std::exception& e) {
            std::cerr << "验证日志条目失败: " << e.what() << std::endl;
        }

        std::cout << "测试完成" << std::endl;
        return saveSuccess ? 0 : 1;
    } catch (const MySQLStorageException& e) {
        std::cerr << "MySQLStorageException: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "std::exception: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::cerr << "未知异常" << std::endl;
        return 1;
    }
}
