#include "xumj/processor/log_processor.h"
#include "xumj/network/tcp_server.h"
#include <nlohmann/json.hpp>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <thread>
#include <fstream>
#include <regex>

using namespace xumj::network;
using namespace xumj::processor;

int main() {
    // 读取配置文件
    LogProcessorConfig config;
    try {
        std::ifstream configFile("../src/processor/config/config.json");
        if (configFile) {
            nlohmann::json j;
            configFile >> j;
            // MySQL配置
            if (j.contains("mysql")) {
                const auto& m = j["mysql"];
                if (m.contains("host")) config.mysqlConfig.host = m["host"].get<std::string>();
                if (m.contains("port")) config.mysqlConfig.port = m["port"].get<int>();
                if (m.contains("username")) config.mysqlConfig.username = m["username"].get<std::string>();
                if (m.contains("password")) config.mysqlConfig.password = m["password"].get<std::string>();
                if (m.contains("database")) config.mysqlConfig.database = m["database"].get<std::string>();
                if (m.contains("table")) config.mysqlConfig.table = m["table"].get<std::string>();
            }
            // Redis配置
            if (j.contains("redis")) {
                const auto& r = j["redis"];
                if (r.contains("host")) config.redisConfig.host = r["host"].get<std::string>();
                if (r.contains("port")) config.redisConfig.port = r["port"].get<int>();
                if (r.contains("password")) config.redisConfig.password = r["password"].get<std::string>();
                if (r.contains("database")) config.redisConfig.database = r["database"].get<int>();
            }
            config.enableMySQLStorage = true;
            config.enableRedisStorage = true;
        } else {
            std::cerr << "未找到配置文件 ../src/processor/config/config.json，使用默认参数。" << std::endl;
        }
    } catch (const std::exception& e) {
        std::cerr << "读取配置文件出错: " << e.what() << std::endl;
    }
    // 其他参数可按需从配置文件读取或用默认值
    config.debug = true;
    config.workerThreads = 4;
    config.queueSize = 1000;
    config.tcpPort = 9001;
    // 输出配置信息
    std::cout << "【配置文件加载成功】MySQL: " << config.mysqlConfig.host << ":" << config.mysqlConfig.port
              << " 用户: " << config.mysqlConfig.username << " 数据库: " << config.mysqlConfig.database << std::endl;
    std::cout << "【配置文件加载成功】Redis: " << config.redisConfig.host << ":" << config.redisConfig.port << std::endl;
    std::cout << "main: config.mysqlConfig.table = " << config.mysqlConfig.table << std::endl;
    LogProcessor processor(config);
    // 添加解析器
    auto jsonParser = std::make_shared<JsonLogParser>();
    jsonParser->SetConfig(config);
    processor.AddLogParser(jsonParser);
    // 启动处理器
    if (!processor.Start()) {
        std::cerr << "LogProcessor启动失败" << std::endl;
        return 1;
    }
    std::cout << "【MySQL/Redis连接成功】LogProcessor已启动！" << std::endl;
    // 启动TcpServer
    TcpServer server("ProcessorServer", "0.0.0.0", 9001, 4);
    server.SetMessageCallback([&processor](uint64_t connId, const std::string& msg, muduo::Timestamp){
        (void)connId;
        // 反序列化JSON数组
        auto logs = nlohmann::json::parse(msg, nullptr, false);
        if (!logs.is_array()) return;
        for (const auto& log : logs) {
            // 优先读取新字段
            std::string msgStr = log.value("message", "");
            std::string timeStr = log.value("timestamp", "");
            std::string levelStr = log.value("level", "");
            std::string sourceStr = log.value("source", "collector");

            // 兼容老字段
            if (msgStr.empty()) msgStr = log.value("content", "");
            if (timeStr.empty()) timeStr = log.value("time", "");

            LogData data;
            data.message = msgStr;
            data.id = log.value("id", GenerateUUID());
            data.source = sourceStr;

            // 解析时间
            std::tm tm = {};
            std::istringstream ss(timeStr);
            ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
            if (!ss.fail()) {
                data.timestamp = std::chrono::system_clock::from_time_t(std::mktime(&tm));
            } else {
                data.timestamp = std::chrono::system_clock::now();
            }

            // level写入metadata，便于解析器使用
            if (!levelStr.empty()) {
                data.metadata["level"] = levelStr;
            }

            processor.SubmitLogData(data);
        }
    });
    server.Start();
    std::cout << "ProcessorServer已启动，监听9001端口..." << std::endl;
    while (true) std::this_thread::sleep_for(std::chrono::seconds(10));
    return 0;
} 