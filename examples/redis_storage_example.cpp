#include <xumj/storage/redis_storage.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <vector>
#include <string>
#include <stdexcept>

using namespace xumj::storage;

// 工具函数：打印Redis操作结果
void printResult(const std::string& operation, bool success) {
    std::cout << operation << ": " << (success ? "成功" : "失败") << std::endl;
}

// 工具函数：打印分隔线
void printSeparator() {
    std::cout << "\n" << std::string(50, '-') << "\n" << std::endl;
}

int main(int argc, char** argv) {
    // 标记参数已使用，避免编译警告
    (void)argc;
    (void)argv;
    
    try {
        std::cout << "Redis存储示例程序启动...\n" << std::endl;
        
        // 配置Redis
        RedisConfig config;
        config.host = "127.0.0.1";
        config.port = 6379;
        config.password = "123465";  // 使用Redis配置文件中的密码
        config.database = 0;
        config.timeout = 5000; // 毫秒
        config.poolSize = 5;   // 连接池大小
        
        // 从命令行参数解析配置
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--host" && i + 1 < argc) {
                config.host = argv[++i];
            } else if (arg == "--port" && i + 1 < argc) {
                config.port = std::stoi(argv[++i]);
            } else if (arg == "--p" && i + 1 < argc) {
                config.password = argv[++i];
            } else if (arg == "--db" && i + 1 < argc) {
                config.database = std::stoi(argv[++i]);
            } else if(arg == "--timeout" && i + 1 < argc){
                config.timeout = std::stoi(argv[++i]);
            } else if(arg == "--poolSize" && i +1 < argc){
                config.poolSize = std::stoi(argv[++i]);
            }
        }
        
        std::cout << "连接Redis服务器: " << config.host << ":" << config.port << std::endl;
        
        // 创建Redis存储实例
        RedisStorage redis(config);
        
        // 测试连接
        if (redis.Ping()) {
            std::cout << "Redis连接测试：成功" << std::endl;
            std::cout << "Redis服务器信息：" << std::endl;
            
            // 获取服务器信息的简短摘要
            std::string info = redis.Info();
            size_t pos = 0;
            std::string line;
            int lines = 0;
            
            while ((pos = info.find('\n')) != std::string::npos && lines < 10) {
                line = info.substr(0, pos);
                if (!line.empty() && line[0] != '#') {
                    std::cout << "  " << line << std::endl;
                    lines++;
                }
                info.erase(0, pos + 1);
            }
        } else {
            std::cerr << "Redis连接测试：失败" << std::endl;
            return 1;
        }
        
        printSeparator();
        std::cout << "1. 基本字符串操作" << std::endl;
        
        // 设置和获取字符串
        std::string testKey = "test:string";
        std::string testValue = "Hello, Redis!";
        
        printResult("SET " + testKey, redis.Set(testKey, testValue));
        std::string value = redis.Get(testKey);
        std::cout << "GET " << testKey << ": " << value << std::endl;
        
        // 设置过期时间
        printResult("EXPIRE " + testKey + " 5s", redis.Expire(testKey, 5));
        std::cout << "键存在: " << (redis.Exists(testKey) ? "是" : "否") << std::endl;
        
        std::cout << "等待6秒..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(6));
        std::cout << "键存在: " << (redis.Exists(testKey) ? "是" : "否") << std::endl;
        
        printSeparator();
        std::cout << "2. 列表操作" << std::endl;
        
        // 列表操作
        std::string listKey = "test:list";
        redis.Delete(listKey); // 确保列表为空
        
        std::cout << "LPUSH " << listKey << std::endl;
        for (int i = 1; i <= 5; ++i) {
            int len = redis.ListPushFront(listKey, "列表项-" + std::to_string(i));
            std::cout << "  添加项 " << i << "，当前长度: " << len << std::endl;
        }
        
        std::cout << "RPUSH " << listKey << std::endl;
        for (int i = 6; i <= 10; ++i) {
            int len = redis.ListPush(listKey, "列表项-" + std::to_string(i));
            std::cout << "  添加项 " << i << "，当前长度: " << len << std::endl;
        }
        
        std::cout << "LLEN " << listKey << ": " << redis.ListLength(listKey) << std::endl;
        
        std::cout << "LRANGE " << listKey << " 0 -1:" << std::endl;
        auto list = redis.ListRange(listKey, 0, -1);
        for (const auto& item : list) {
            std::cout << "  " << item << std::endl;
        }
        
        std::string popped = redis.ListPopFront(listKey);
        std::cout << "LPOP " << listKey << ": " << popped << std::endl;
        
        popped = redis.ListPop(listKey);
        std::cout << "RPOP " << listKey << ": " << popped << std::endl;
        
        printSeparator();
        std::cout << "3. 哈希表操作" << std::endl;
        
        // 哈希表操作
        std::string hashKey = "test:hash";
        redis.Delete(hashKey); // 确保哈希表为空
        
        printResult("HSET " + hashKey + " name 张三", redis.HashSet(hashKey, "name", "张三"));
        printResult("HSET " + hashKey + " age 30", redis.HashSet(hashKey, "age", "30"));
        printResult("HSET " + hashKey + " city 北京", redis.HashSet(hashKey, "city", "北京"));
        
        std::cout << "HGET " << hashKey << " name: " << redis.HashGet(hashKey, "name") << std::endl;
        std::cout << "HGET " << hashKey << " age: " << redis.HashGet(hashKey, "age") << std::endl;
        std::cout << "HGET " << hashKey << " city: " << redis.HashGet(hashKey, "city") << std::endl;
        
        std::cout << "HEXISTS " << hashKey << " name: " << (redis.HashExists(hashKey, "name") ? "是" : "否") << std::endl;
        std::cout << "HEXISTS " << hashKey << " gender: " << (redis.HashExists(hashKey, "gender") ? "是" : "否") << std::endl;
        
        printResult("HDEL " + hashKey + " city", redis.HashDelete(hashKey, "city"));
        
        std::cout << "HGETALL " << hashKey << ":" << std::endl;
        auto hash = redis.HashGetAll(hashKey);
        for (const auto& pair : hash) {
            std::cout << "  " << pair.first << ": " << pair.second << std::endl;
        }
        
        printSeparator();
        std::cout << "4. 集合操作" << std::endl;
        
        // 集合操作
        std::string setKey = "test:set";
        redis.Delete(setKey); // 确保集合为空
        
        std::cout << "SADD " << setKey << std::endl;
        for (const auto& item : {"苹果", "香蕉", "橙子", "葡萄", "苹果"}) {
            int added = redis.SetAdd(setKey, item);
            std::cout << "  添加 " << item << "，结果: " << (added ? "新增" : "已存在") << std::endl;
        }
        
        std::cout << "SCARD " << setKey << ": " << redis.SetSize(setKey) << std::endl;
        
        std::cout << "SISMEMBER " << setKey << " 香蕉: " << (redis.SetIsMember(setKey, "香蕉") ? "是" : "否") << std::endl;
        std::cout << "SISMEMBER " << setKey << " 西瓜: " << (redis.SetIsMember(setKey, "西瓜") ? "是" : "否") << std::endl;
        
        std::cout << "SMEMBERS " << setKey << ":" << std::endl;
        auto members = redis.SetMembers(setKey);
        for (const auto& member : members) {
            std::cout << "  " << member << std::endl;
        }
        
        printResult("SREM " + setKey + " 橙子", redis.SetRemove(setKey, "橙子") > 0);
        
        printSeparator();
        std::cout << "5. 事务操作" << std::endl;
        
        // 事务操作
        std::string txKey1 = "test:tx:1";
        std::string txKey2 = "test:tx:2";
        
        redis.Delete(txKey1);
        redis.Delete(txKey2);
        
        try {
            printResult("MULTI", redis.Multi());
            redis.Set(txKey1, "事务测试1");
            redis.Set(txKey2, "事务测试2", 30); // 30秒过期
            redis.ListPush("test:tx:list", "事务列表项");
            
            std::cout << "执行事务..." << std::endl;
            auto results = redis.Exec();
            std::cout << "事务结果数量: " << results.size() << std::endl;
            
            // 显示事务结果
            for (size_t i = 0; i < results.size(); i++) {
                std::cout << "  结果 " << (i+1) << ": " << results[i] << std::endl;
            }
            
            std::cout << "事务后检查:" << std::endl;
            std::cout << "GET " << txKey1 << ": " << redis.Get(txKey1) << std::endl;
            std::cout << "GET " << txKey2 << ": " << redis.Get(txKey2) << std::endl;
        } catch (const RedisStorageException& e) {
            std::cout << "Redis事务操作失败: " << e.what() << std::endl;
            std::cout << "注意: Redis事务需要在同一连接上执行MULTI和EXEC命令，这在连接池环境下可能会出现问题" << std::endl;
        } catch (const std::invalid_argument& e) {
            std::cout << "数据转换错误: " << e.what() << std::endl;
            std::cout << "可能是尝试将非数字结果转换为数字" << std::endl;
        } catch (const std::out_of_range& e) {
            std::cout << "数值超出范围: " << e.what() << std::endl;
        } catch (const std::exception& e) {
            std::cout << "事务操作发生未知异常: " << e.what() << std::endl;
        }
        
        printSeparator();
        std::cout << "清理测试数据..." << std::endl;
        
        // 清理
        redis.Delete(testKey);
        redis.Delete(listKey);
        redis.Delete(hashKey);
        redis.Delete(setKey);
        redis.Delete(txKey1);
        redis.Delete(txKey2);
        redis.Delete("test:tx:list");
        
        std::cout << "Redis存储示例程序执行完毕。" << std::endl;
        
    } catch (const RedisStorageException& e) {
        std::cerr << "Redis错误: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
} 