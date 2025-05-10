#include <iostream>
#include <cstring>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <csignal>
#include <vector>
#include <fstream>
#include <iomanip>
#include "log_generator.h"
#include "xumj/network/tcp_client.h"

using namespace xumj::tools;
using namespace xumj::network;

// 全局变量，用于处理信号
std::atomic<bool> running(true);

// 信号处理函数
void signalHandler(int signum) {
    std::cout << "接收到信号 " << signum << std::endl;
    running = false;
}

// 日志回调函数，用于计数和输出
void logCallback(const std::string& /* content */) {
    static std::atomic<std::size_t> count(0);
    static auto startTime = std::chrono::steady_clock::now();
    
    std::size_t current = ++count;
    if (current % 1000 == 0) { // 每1000条日志输出一次
        auto now = std::chrono::steady_clock::now();
        auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - startTime).count();
        std::cout << "已生成 " << current << " 条日志, 耗时 " << (elapsed / 1000.0) << " 秒" << std::endl;
    }
}

// 网络发送回调函数
std::shared_ptr<TcpClient> networkClient;
void networkSendCallback(const std::string& content) {
    if (networkClient && networkClient->IsConnected()) {
        networkClient->Send(content);
    }
}

int main(int argc, char* argv[]) {
    // 注册信号处理器
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // 参数默认值
    int logsPerSecond = 10;  // 每秒生成的日志数
    int durationSeconds = 5; // 运行时间（秒）
    int complexity = 5;      // 日志复杂度（1-10）
    bool outputToConsole = false;  // 是否输出到控制台
    bool outputToFile = false;     // 是否输出到文件
    bool outputToNetwork = false;  // 是否输出到网络
    std::string targetHost = "127.0.0.1";
    int targetPort = 8001;
    std::string outputFile = "generated_logs.txt";
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "--help" || arg == "-h") {
            std::cout << "用法: " << argv[0] << " [选项]" << std::endl;
            std::cout << "选项:" << std::endl;
            std::cout << "  --rate <number>           每秒生成的日志数量（默认: 10）" << std::endl;
            std::cout << "  --duration <seconds>      运行时间，秒（默认: 5）" << std::endl;
            std::cout << "  --complexity <1-10>       日志内容复杂度（默认: 5）" << std::endl;
            std::cout << "  --console                 输出到控制台" << std::endl;
            std::cout << "  --file [filename]         输出到文件（默认文件名: generated_logs.txt）" << std::endl;
            std::cout << "  --network                 输出到网络" << std::endl;
            std::cout << "  --target <host:port>      网络目标地址（默认: 127.0.0.1:8001）" << std::endl;
            std::cout << "  --help                    显示此帮助信息" << std::endl;
            return 0;
        } else if (arg == "--rate" && i + 1 < argc) {
            logsPerSecond = std::stoi(argv[++i]);
        } else if (arg == "--duration" && i + 1 < argc) {
            durationSeconds = std::stoi(argv[++i]);
        } else if (arg == "--complexity" && i + 1 < argc) {
            complexity = std::stoi(argv[++i]);
            if (complexity < 1) complexity = 1;
            if (complexity > 10) complexity = 10;
        } else if (arg == "--console") {
            outputToConsole = true;
        } else if (arg == "--file") {
            outputToFile = true;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                outputFile = argv[++i];
            }
        } else if (arg == "--network") {
            outputToNetwork = true;
        } else if (arg == "--target" && i + 1 < argc) {
            std::string target = argv[++i];
            size_t colonPos = target.find(':');
            if (colonPos != std::string::npos) {
                targetHost = target.substr(0, colonPos);
                targetPort = std::stoi(target.substr(colonPos + 1));
            } else {
                targetHost = target;
            }
        }
    }
    
    // 如果没有指定任何输出方法，默认输出到控制台
    if (!outputToConsole && !outputToFile && !outputToNetwork) {
        outputToConsole = true;
    }
    
    // 创建日志生成器配置
    LogGeneratorConfig config;
    config.logsPerSecond = logsPerSecond;
    config.batchSize = std::max(1, logsPerSecond / 10); // 批次大小为每秒日志数的1/10，至少为1
    config.duration = std::chrono::seconds(durationSeconds);
    config.contentComplexity = complexity;
    config.outputToConsole = outputToConsole;
    config.outputToFile = outputToFile;
    config.outputFilePath = outputFile;
    config.outputToNetwork = outputToNetwork;
    config.targetHost = targetHost;
    config.targetPort = targetPort;
    
    // 输出配置信息
    std::cout << "===== 日志生成器配置 =====" << std::endl;
    std::cout << "生成速率: " << logsPerSecond << " 条/秒" << std::endl;
    std::cout << "运行时间: " << durationSeconds << " 秒" << std::endl;
    std::cout << "批处理大小: " << config.batchSize << " 条/批" << std::endl;
    std::cout << "内容复杂度: " << complexity << std::endl;
    std::cout << "输出到控制台: " << (outputToConsole ? "是" : "否") << std::endl;
    std::cout << "输出到文件: " << (outputToFile ? "是" : "否") << std::endl;
    if (outputToFile) {
        std::cout << "输出文件: " << outputFile << std::endl;
    }
    std::cout << "输出到网络: " << (outputToNetwork ? "是" : "否") << std::endl;
    if (outputToNetwork) {
        std::cout << "网络目标: " << targetHost << ":" << targetPort << std::endl;
    }
    std::cout << "==========================" << std::endl;
    
    // 如果要输出到网络，创建TCP客户端
    if (outputToNetwork) {
        // 创建并连接TCP客户端
        networkClient = std::make_shared<TcpClient>("LogGeneratorClient", targetHost, targetPort, true);
        
        // 设置连接回调
        networkClient->SetConnectionCallback([&](bool connected) {
            if (connected) {
                std::cout << "已连接到服务器: " << targetHost << ":" << targetPort << std::endl;
            } else {
                std::cout << "与服务器断开连接: " << targetHost << ":" << targetPort << std::endl;
                // 如果连接断开且生成器仍在运行，尝试重新连接
                if (running) {
                    std::cout << "尝试重新连接..." << std::endl;
                    networkClient->Connect();
                }
            }
        });
        
        // 设置消息回调（可选，用于处理服务器响应）
        networkClient->SetMessageCallback([](const std::string& message, muduo::Timestamp) {
            std::cout << "收到服务器响应: " << message << std::endl;
        });
        
        // 连接服务器
        networkClient->Connect();
        
        // 等待连接建立，最多等待3秒
        for (int i = 0; i < 30 && running; ++i) {
            if (networkClient->IsConnected()) {
                break;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        
        if (!networkClient->IsConnected()) {
            std::cerr << "无法连接到服务器，退出程序" << std::endl;
            return 1;
        }
    }
    
    // 创建日志生成器
    LogGenerator generator(config);
    
    // 设置回调函数
    if (outputToNetwork) {
        // 如果输出到网络，使用网络发送回调
        generator.SetLogCallback(networkSendCallback);
    } else {
        // 否则使用普通回调
        generator.SetLogCallback(logCallback);
    }
    
    // 开始生成日志
    std::cout << "开始生成日志..." << std::endl;
    generator.Start();
    
    // 监控生成进度
    std::size_t prevCount = 0;
    
    while (running && generator.GetGeneratedLogCount() < static_cast<uint64_t>(logsPerSecond * durationSeconds)) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        std::size_t count = generator.GetGeneratedLogCount();
        double elapsedSeconds = static_cast<double>(count) / logsPerSecond;
        double progress = (logsPerSecond * durationSeconds) > 0 ? 
            static_cast<double>(count) / (logsPerSecond * durationSeconds) * 100.0 : 0.0;
        
        std::cout << "已生成: " << count << " 条日志 | " 
                  << elapsedSeconds << "/" << durationSeconds << " 秒 | " 
                  << progress << "%" << std::endl;
                  
        // 检查是否生成速率过慢
        if (count - prevCount < logsPerSecond * 0.5 && count < static_cast<size_t>(logsPerSecond * durationSeconds)) {
            std::cout << "警告: 生成速率低于预期，请考虑降低目标速率" << std::endl;
        }
        
        prevCount = count;
    }
    
    // 停止生成器
    generator.Stop();
    
    // 等待生成器完全停止
    generator.Wait();
    
    // 关闭网络连接
    if (networkClient) {
        networkClient->Disconnect();
        networkClient.reset();
    }
    
    // 输出最终统计信息
    auto finalCount = generator.GetGeneratedLogCount();
    double actualRate = durationSeconds > 0 ? static_cast<double>(finalCount) / durationSeconds : 0.0;
    
    std::cout << "日志生成完成！" << std::endl;
    std::cout << "实际生成: " << finalCount << " 条日志" << std::endl;
    std::cout << "实际速率: " << std::fixed << std::setprecision(2) << actualRate << " 条/秒" << std::endl;
    
    return 0;
} 