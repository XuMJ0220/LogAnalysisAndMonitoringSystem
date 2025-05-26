#include <xumj/storage/mysql_storage.h>
#include <iostream>
#include <iomanip>
#include <vector>
#include <string>
#include <stdexcept>
#include <chrono>
#include <thread>
#include <ctime>
#include <random>
#include <sstream>

using namespace xumj::storage;

// 工具函数：获取当前时间的ISO8601字符串
std::string getCurrentTimeIso8601() {
    auto now = std::chrono::system_clock::now();
    auto now_c = std::chrono::system_clock::to_time_t(now);
    std::tm tm = *std::localtime(&now_c);
    
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%dT%H:%M:%S");
    
    // 添加毫秒部分
    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch() % std::chrono::seconds(1));
    oss << "." << std::setfill('0') << std::setw(3) << ms.count();
    
    return oss.str();
}

// 工具函数：生成随机日志级别
std::string getRandomLogLevel() {
    static const std::vector<std::string> levels = {"INFO", "DEBUG", "WARNING", "ERROR", "CRITICAL"};
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, levels.size() - 1);
    return levels[dis(gen)];
}

// 工具函数：生成随机日志来源
std::string getRandomLogSource() {
    static const std::vector<std::string> sources = {
        "app-server", "web-frontend", "database", "cache", "auth-service",
        "payment-gateway", "notification-service", "user-service", "analytics"
    };
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> dis(0, sources.size() - 1);
    return sources[dis(gen)];
}

// 工具函数：生成随机日志消息
std::string getRandomLogMessage() {
    static const std::vector<std::string> templates = {
        "用户 %d 登录系统",
        "处理请求 #%d 完成，耗时 %d ms",
        "数据库查询执行时间: %d ms",
        "API调用成功，响应代码: %d",
        "缓存命中率: %d%%",
        "系统内存使用率: %d%%",
        "接收到来自客户端 %d 的请求",
        "发送了 %d 条通知",
        "用户 %d 更新了个人资料",
        "完成了 ID 为 %d 的任务",
        "事务 #%d 已提交",
        "队列 #%d 中有 %d 个待处理项"
    };
    
    static std::random_device rd;
    static std::mt19937 gen(rd());
    static std::uniform_int_distribution<> template_dis(0, templates.size() - 1);
    static std::uniform_int_distribution<> request_dis(1, 1000);
    static std::uniform_int_distribution<> id_dis(1000, 9999);
    static std::uniform_int_distribution<> time_dis(1, 1000);
    static std::uniform_int_distribution<> code_dis(200, 204);
    static std::uniform_int_distribution<> percent_dis(0, 100);
    static std::uniform_int_distribution<> count_dis(1, 100);
    
    std::string templ = templates[template_dis(gen)];
    char buffer[256];
    
    if (templ.find("%d%%") != std::string::npos) {
        // 百分比模板
        snprintf(buffer, sizeof(buffer), templ.c_str(), percent_dis(gen));
    } else if (templ.find("响应代码") != std::string::npos) {
        // 响应代码模板
        snprintf(buffer, sizeof(buffer), templ.c_str(), code_dis(gen));
    } else if (templ.find("ms") != std::string::npos) {
        // 时间模板
        if(templ.find("处理请求")!=std::string::npos){
            snprintf(buffer,sizeof(buffer),templ.c_str(),request_dis(gen),time_dis(gen));
        }else{
            snprintf(buffer, sizeof(buffer), templ.c_str(), time_dis(gen));
        }
    } else if (templ.find("队列") != std::string::npos) {
        // 双参数模板
        snprintf(buffer, sizeof(buffer), templ.c_str(), id_dis(gen), count_dis(gen));
    } else {
        // 普通ID模板
        snprintf(buffer, sizeof(buffer), templ.c_str(), id_dis(gen));
    }
    
    return std::string(buffer);
}

// 工具函数：打印分隔线
void printSeparator() {
    std::cout << "\n" << std::string(70, '-') << "\n" << std::endl;
}

const int numEntries = 100000;

int main(int argc, char** argv) {
    // 标记参数已使用，避免编译警告
    (void)argc;
    (void)argv;
    
    try {
        std::cout << "MySQL存储示例程序启动...\n" << std::endl;
        
        // 配置MySQL
        MySQLConfig config;
        config.host = "127.0.0.1";
        config.port = 3306;
        config.username = "root";
        config.password = "ytfhqqkso1"; 
        config.database = "storage_example";  // 测试数据库
        config.timeout = 5;  // 秒
        config.poolSize = 5; // 连接池大小
        
        // 从命令行参数解析配置
        //例子 ./mysql_storage_example --host 127.0.0.1 --port 3306 --user root --p ytfhqqkso1 --db storage_example --timeout 5 --poolSize 5
        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "--host" && i + 1 < argc) {
                config.host = argv[++i];
            } else if (arg == "--port" && i + 1 < argc) {
                config.port = std::stoi(argv[++i]);
            } else if (arg == "--user" && i + 1 < argc) {
                config.username = argv[++i];
            } else if (arg == "--p" && i + 1 < argc) {
                config.password = argv[++i];
            } else if (arg == "--db" && i + 1 < argc) {
                config.database = argv[++i];
            }else if(arg == "--timeout" && i + 1 < argc){
                config.timeout = std::stoi(argv[++i]);
            }else if(arg == "--poolSize" && i + 1 < argc){
                config.poolSize = std::stoi(argv[++i]);
            }
        }
        
        std::cout << "连接MySQL服务器: " << config.host << ":" << config.port << std::endl;
        std::cout << "用户名: " << config.username << std::endl;
        std::cout << "数据库: " << config.database << std::endl;
        
        // 创建MySQL存储实例
        MySQLStorage mysql(config);
        
        // 测试连接
        if (mysql.TestConnection()) {
            std::cout << "MySQL连接测试: 成功" << std::endl;
        } else {
            std::cerr << "MySQL连接测试: 失败" << std::endl;
            return 1;
        }
        
        // 初始化数据库表结构
        std::cout << "初始化数据库表结构..." << std::endl;
        if (mysql.Initialize()) {
            std::cout << "数据库表结构初始化成功" << std::endl;
        } else {
            std::cerr << "数据库表结构初始化失败" << std::endl;
            return 1;
        }
        
        printSeparator();
        std::cout << "1. 插入单条日志记录" << std::endl;
        
        // 创建日志条目
        MySQLStorage::LogEntry entry;
        entry.id = "log-" + std::to_string(std::time(nullptr));
        entry.timestamp = getCurrentTimeIso8601();
        entry.level = "INFO";
        entry.source = "mysql-example";
        entry.message = "这是一条测试日志消息";
        entry.fields["user_id"] = "12345";
        entry.fields["module"] = "test-module";
        entry.fields["action"] = "initialize";
        
        // 保存日志条目
        bool saved = mysql.SaveLogEntry(entry);
        std::cout << "保存日志条目: " << (saved ? "成功" : "失败") << std::endl;
        
        // 通过ID检索日志条目
        std::cout << "通过ID检索日志条目..." << std::endl;
        auto retrievedEntry = mysql.GetLogEntryById(entry.id);
        
        if (!retrievedEntry.id.empty()) {
            std::cout << "检索到日志条目:" << std::endl;
            std::cout << "  ID: " << retrievedEntry.id << std::endl;
            std::cout << "  时间戳: " << retrievedEntry.timestamp << std::endl;
            std::cout << "  级别: " << retrievedEntry.level << std::endl;
            std::cout << "  来源: " << retrievedEntry.source << std::endl;
            std::cout << "  消息: " << retrievedEntry.message << std::endl;
            std::cout << "  字段:" << std::endl;
            for (const auto& field : retrievedEntry.fields) {
                std::cout << "    " << field.first << ": " << field.second << std::endl;
            }
        } else {
            std::cerr << "未找到日志条目" << std::endl;
        }
        
        printSeparator();
        std::cout << "2. 批量插入日志记录" << std::endl;
        
        // 创建多条日志条目
        std::vector<MySQLStorage::LogEntry> entries;
        
        std::cout << "生成 " << numEntries << " 条随机日志记录..." << std::endl;
        
        // 生成一些随机日志记录
        for (int i = 0; i < numEntries; ++i) {
            MySQLStorage::LogEntry e;
            e.id = "batch-log-" + std::to_string(std::time(nullptr)) + "-" + std::to_string(i);
            
            // 随机生成不同时间点
            auto now = std::chrono::system_clock::now();
            auto random_time = now - std::chrono::minutes(std::rand() % 60);
            auto random_time_t = std::chrono::system_clock::to_time_t(random_time);
            std::tm random_tm = *std::localtime(&random_time_t);
            
            std::ostringstream oss;
            oss << std::put_time(&random_tm, "%Y-%m-%dT%H:%M:%S");
            e.timestamp = oss.str();
            
            e.level = getRandomLogLevel();
            e.source = getRandomLogSource();
            e.message = getRandomLogMessage();
            
            // 添加一些随机字段
            e.fields["request_id"] = "req-" + std::to_string(1000 + i);
            e.fields["process_time_ms"] = std::to_string(std::rand() % 500);
            
            if (e.level == "ERROR" || e.level == "CRITICAL") {
                e.fields["error_code"] = std::to_string(1000 + (std::rand() % 100));
                e.fields["stacktrace"] = "at Method" + std::to_string(std::rand() % 10) + 
                                        " in Class" + std::to_string(std::rand() % 5) + 
                                        ".java:line " + std::to_string(100 + (std::rand() % 900));
            }
            
            entries.push_back(e);
        }
        
        // 批量保存日志条目
        int savedCount = mysql.SaveLogEntries(entries);
        std::cout << "已保存 " << savedCount << " / " << entries.size() << " 条日志记录" << std::endl;
        
        printSeparator();
        std::cout << "3. 查询日志记录" << std::endl;
        
        // 获取日志条目总数
        int totalEntries = mysql.GetLogEntryCount();
        std::cout << "数据库中共有 " << totalEntries << " 条日志记录" << std::endl;
        
        // 按级别查询
        for (const auto& level : {"INFO", "WARNING", "ERROR", "CRITICAL"}) {
            auto levelEntries = mysql.QueryLogEntriesByLevel(level, 100000);
            std::cout << "\n级别为 " << level << " 的日志记录 (最多100000条):" << std::endl;
            for (const auto& e : levelEntries) {
                std::cout << "  [" << e.timestamp << "] " << e.source << " - " << e.message << std::endl;
            }
        }
        
        // 按来源查询
        std::cout << "\n按来源查询..." << std::endl;
        auto sourceEntries = mysql.QueryLogEntriesBySource("mysql-example", 100000);
        std::cout << "来源为 'mysql-example' 的日志记录 (最多100000条):" << std::endl;
        for (const auto& e : sourceEntries) {
            std::cout << "  [" << e.timestamp << "] " << e.level << " - " << e.message << std::endl;
        }
        
        // 按时间范围查询
        std::cout << "\n按时间范围查询..." << std::endl;
        auto now = std::chrono::system_clock::now();
        auto oneHourAgo = now - std::chrono::hours(1);
        
        auto now_t = std::chrono::system_clock::to_time_t(now);
        auto oneHourAgo_t = std::chrono::system_clock::to_time_t(oneHourAgo);
        
        std::tm now_tm = *std::localtime(&now_t);
        std::tm oneHourAgo_tm = *std::localtime(&oneHourAgo_t);
        
        std::ostringstream now_oss, oneHourAgo_oss;
        now_oss << std::put_time(&now_tm, "%Y-%m-%dT%H:%M:%S");
        oneHourAgo_oss << std::put_time(&oneHourAgo_tm, "%Y-%m-%dT%H:%M:%S");
        
        std::string nowStr = now_oss.str();
        std::string oneHourAgoStr = oneHourAgo_oss.str();
        
        std::cout << "查询时间范围: " << oneHourAgoStr << " 到 " << nowStr << std::endl;
        
        auto timeRangeEntries = mysql.QueryLogEntriesByTimeRange(oneHourAgoStr, nowStr, 100000);
        std::cout << "最近一小时内的日志记录 (最多100000条):" << std::endl;
        for (const auto& e : timeRangeEntries) {
            std::cout << "  [" << e.timestamp << "] " << e.level << " - " << e.source << " - " << e.message << std::endl;
        }
        
        // 按关键词搜索
        std::cout << "\n按关键词搜索..." << std::endl;
        auto keywordEntries = mysql.SearchLogEntriesByKeyword("用户", 100000);
        std::cout << "包含关键词 '用户' 的日志记录 (最多100000条):" << std::endl;
        for (const auto& e : keywordEntries) {
            std::cout << "  [" << e.timestamp << "] " << e.level << " - " << e.message << std::endl;
        }
        
        printSeparator();
        std::cout << "4. 删除过期日志记录" << std::endl;
        
        // 计算一天前的时间
        auto oneDayAgo = now - std::chrono::hours(24);
        auto oneDayAgo_t = std::chrono::system_clock::to_time_t(oneDayAgo);
        std::tm oneDayAgo_tm = *std::localtime(&oneDayAgo_t);
        
        std::ostringstream oneDayAgo_oss;
        oneDayAgo_oss << std::put_time(&oneDayAgo_tm, "%Y-%m-%dT%H:%M:%S");
        std::string oneDayAgoStr = oneDayAgo_oss.str();
        
        std::cout << "删除 " << oneDayAgoStr << " 之前的所有日志记录..." << std::endl;
        int deletedCount = mysql.DeleteLogEntriesBefore(oneDayAgoStr);
        std::cout << "已删除 " << deletedCount << " 条过期日志记录" << std::endl;
        
        // 再次获取日志条目总数
        totalEntries = mysql.GetLogEntryCount();
        std::cout << "删除后数据库中剩余 " << totalEntries << " 条日志记录" << std::endl;
        
        printSeparator();
        std::cout << "MySQL存储示例程序执行完毕。" << std::endl;
        
    } catch (const MySQLStorageException& e) {
        std::cerr << "MySQL错误: " << e.what() << std::endl;
        return 1;
    } catch (const std::exception& e) {
        std::cerr << "发生异常: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}

