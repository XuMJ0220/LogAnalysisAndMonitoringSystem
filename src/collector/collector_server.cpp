#include <xumj/network/tcp_server.h>
#include <xumj/collector/log_collector.h>
#include <nlohmann/json.hpp>
#include <unordered_map>
#include <memory>
#include <mutex>
#include <iostream>

using json = nlohmann::json;
using namespace xumj::network;
using namespace xumj::collector;

// 管理每个连接的采集器
std::unordered_map<uint64_t, std::unique_ptr<LogCollector>> collectors;
std::mutex collectorsMutex;
TcpServer* g_server = nullptr; // 用于回调中推送日志

// 日志推送回调
void PushLogToClient(uint64_t connId, const std::vector<LogEntry>& entries) {
    json arr = json::array();
    for (const auto& entry : entries) {
        arr.push_back({
            {"time", TimestampToString(entry.GetTimestamp())},
            {"level", LogLevelToString(entry.GetLevel())},
            {"content", entry.GetContent()}
        });
    }
    if (g_server && !arr.empty()) g_server->Send(connId, arr.dump() + "\n");
}

void OnMessage(uint64_t connId, const std::string& msg, muduo::Timestamp) {
    auto j = json::parse(msg, nullptr, false);
    if (!j.is_object()) return;
    if (j["cmd"] == "start") {
        std::string file = j.value("file", "");
        int interval = j.value("interval", 1000);
        int maxLines = j.value("maxLines", 10);
        std::string levelStr = j.value("level", "INFO");
        bool compress = j.value("compress", false);
        std::vector<std::string> keywords;
        if (j.contains("keywords") && j["keywords"].is_array()) {
            for (const auto& kw : j["keywords"]) {
                if (kw.is_string()) keywords.push_back(kw.get<std::string>());
            }
        }
        LogLevel level = LogLevel::INFO;
        if (levelStr == "DEBUG") level = LogLevel::DEBUG;
        else if (levelStr == "WARNING") level = LogLevel::WARNING;
        else if (levelStr == "ERROR") level = LogLevel::ERROR;
        else if (levelStr == "CRITICAL") level = LogLevel::CRITICAL;
        else if (levelStr == "TRACE") level = LogLevel::TRACE;
        // 创建采集器
        auto collector = std::make_unique<LogCollector>();
        CollectorConfig config;
        config.batchSize = 10; // 每10条推送一次
        config.flushInterval = std::chrono::milliseconds(interval);
        config.minLevel = level;
        config.compressLogs = compress;
        collector->Initialize(config);
        // 关键字过滤
        if (!keywords.empty()) {
            collector->AddFilter(std::make_shared<KeywordFilter>(keywords));
        }
        // 设置推送回调
        collector->SetSendCallback([connId](size_t){ /* 可选统计 */ });
        collector->CollectFromFile(file, level, interval, maxLines);
        std::lock_guard<std::mutex> lock(collectorsMutex);
        collectors[connId] = std::move(collector);
        // 在OnMessage中，启动采集前动态设置connId
        RegisterLogPushCallback(PushLogToClient, connId);
    } else if (j["cmd"] == "stop") {
        std::lock_guard<std::mutex> lock(collectorsMutex);
        if (collectors.count(connId)) {
            collectors[connId]->Shutdown();
            collectors.erase(connId);
        }
    }
}

void OnConnection(uint64_t connId, const std::string&, bool connected) {
    if (!connected) {
        std::lock_guard<std::mutex> lock(collectorsMutex);
        if (collectors.count(connId)) {
            collectors[connId]->Shutdown();
            collectors.erase(connId);
        }
    }
}

int main() {
    // 注册日志推送回调
    xumj::collector::RegisterLogPushCallback(PushLogToClient, 0); // connId后续在OnMessage中动态设置
    TcpServer server("CollectorServer", "127.0.0.1", 9000, 4);
    g_server = &server;
    server.SetMessageCallback(OnMessage);
    server.SetConnectionCallback(OnConnection);
    server.Start();
    while (true) std::this_thread::sleep_for(std::chrono::seconds(10));
} 