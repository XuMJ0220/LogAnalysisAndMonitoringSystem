#include "log_generator.h"
#include <iostream>
#include <iomanip>
#include <chrono>
#include <thread>
#include <vector>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <csignal>
#include <cstring>
#include <fstream>
#include <sstream>

#include "xumj/network/tcp_client.h"

using namespace xumj::tools;
using namespace xumj::network;

// 全局变量
std::atomic<bool> running(true);
std::mutex statsMutex;
std::condition_variable statsCv;
std::atomic<uint64_t> totalLogs(0);
std::atomic<uint64_t> successLogs(0);
std::atomic<uint64_t> failedLogs(0);

// 信号处理函数
void signalHandler(int signum) {
    std::cout << "收到信号 " << signum << "，准备退出..." << std::endl;
    running = false;
    statsCv.notify_all();
}

// 统计信息结构体
struct Stats {
    uint64_t totalLogs;
    uint64_t successLogs;
    uint64_t failedLogs;
    double elapsedSeconds;
    double logsPerSecond;
    
    Stats() : totalLogs(0), successLogs(0), failedLogs(0), elapsedSeconds(0), logsPerSecond(0) {}
};

// 打印使用方法
void printUsage(const char* programName) {
    std::cout << "用法: " << programName << " [选项]" << std::endl;
    std::cout << "选项:" << std::endl;
    std::cout << "  --rate N        每秒生成的日志数量 (默认: 1000)" << std::endl;
    std::cout << "  --duration N    运行的秒数 (默认: 60)" << std::endl;
    std::cout << "  --threads N     使用的线程数 (默认: 4)" << std::endl;
    std::cout << "  --target host:port  发送日志的目标服务器 (默认: 127.0.0.1:8000)" << std::endl;
    std::cout << "  --complexity N  日志复杂度 (1-10, 默认: 5)" << std::endl;
    std::cout << "  --console       输出到控制台" << std::endl;
    std::cout << "  --file          输出到文件" << std::endl;
    std::cout << "  --network       输出到网络" << std::endl;
    std::cout << "  --report file   输出性能报告到文件" << std::endl;
    std::cout << "  --help          显示此帮助信息" << std::endl;
}

// 解析命令行参数
bool parseCommandLine(int argc, char* argv[], 
                     int& logsPerSecond, 
                     int& durationSeconds, 
                     int& numThreads,
                     std::string& targetHost,
                     int& targetPort,
                     int& complexity,
                     bool& outputToConsole,
                     bool& outputToFile,
                     bool& outputToNetwork,
                     std::string& reportFile) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--rate" && i + 1 < argc) {
            logsPerSecond = std::stoi(argv[++i]);
            if (logsPerSecond <= 0) {
                std::cerr << "错误: 日志生成速率必须是正数" << std::endl;
                return false;
            }
        } else if (arg == "--duration" && i + 1 < argc) {
            durationSeconds = std::stoi(argv[++i]);
            if (durationSeconds <= 0) {
                std::cerr << "错误: 持续时间必须是正数" << std::endl;
                return false;
            }
        } else if (arg == "--threads" && i + 1 < argc) {
            numThreads = std::stoi(argv[++i]);
            if (numThreads <= 0) {
                std::cerr << "错误: 线程数必须是正数" << std::endl;
                return false;
            }
        } else if (arg == "--target" && i + 1 < argc) {
            std::string target = argv[++i];
            size_t pos = target.find(':');
            if (pos != std::string::npos) {
                targetHost = target.substr(0, pos);
                targetPort = std::stoi(target.substr(pos + 1));
            } else {
                std::cerr << "错误: 目标格式应为 host:port" << std::endl;
                return false;
            }
        } else if (arg == "--complexity" && i + 1 < argc) {
            complexity = std::stoi(argv[++i]);
            if (complexity < 1 || complexity > 10) {
                std::cerr << "错误: 复杂度必须在1到10之间" << std::endl;
                return false;
            }
        } else if (arg == "--console") {
            outputToConsole = true;
        } else if (arg == "--file") {
            outputToFile = true;
        } else if (arg == "--network") {
            outputToNetwork = true;
        } else if (arg == "--report" && i + 1 < argc) {
            reportFile = argv[++i];
        } else if (arg == "--help") {
            printUsage(argv[0]);
            return false;
        } else {
            std::cerr << "未知选项: " << arg << std::endl;
            printUsage(argv[0]);
            return false;
        }
    }
    
    return true;
}

// 统计线程函数
void statsThread(int logsPerSecond, int durationSeconds, const std::string& reportFile) {
    auto startTime = std::chrono::steady_clock::now();
    std::vector<Stats> statsHistory;
    
    // 每秒更新一次统计信息
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        
        auto currentTime = std::chrono::steady_clock::now();
        std::chrono::duration<double> elapsed = currentTime - startTime;
        
        Stats stats;
        stats.totalLogs = totalLogs.load();
        stats.successLogs = successLogs.load();
        stats.failedLogs = failedLogs.load();
        stats.elapsedSeconds = elapsed.count();
        stats.logsPerSecond = stats.totalLogs / stats.elapsedSeconds;
        
        // 保存统计信息
        statsHistory.push_back(stats);
        
        // 打印进度
        double progress = std::min(100.0, (elapsed.count() / durationSeconds) * 100.0);
        std::cout << "\r已生成: " << stats.totalLogs << " 条日志 | "
                  << "成功: " << stats.successLogs << " | "
                  << "失败: " << stats.failedLogs << " | "
                  << std::fixed << std::setprecision(1) << stats.elapsedSeconds << "/" << durationSeconds << " 秒 | "
                  << std::fixed << std::setprecision(1) << progress << "% | "
                  << "速率: " << std::fixed << std::setprecision(1) << stats.logsPerSecond << " 条/秒" << std::flush;
        
        // 检查是否达到了持续时间
        if (elapsed.count() >= durationSeconds) {
            running = false;
            break;
        }
    }
    std::cout << std::endl;
    
    // 结束后打印统计信息
    auto endTime = std::chrono::steady_clock::now();
    std::chrono::duration<double> totalElapsed = endTime - startTime;
    
    Stats finalStats;
    finalStats.totalLogs = totalLogs.load();
    finalStats.successLogs = successLogs.load();
    finalStats.failedLogs = failedLogs.load();
    finalStats.elapsedSeconds = totalElapsed.count();
    finalStats.logsPerSecond = finalStats.totalLogs / finalStats.elapsedSeconds;
    
    std::cout << "\n===== 性能测试结果 =====" << std::endl;
    std::cout << "总共生成: " << finalStats.totalLogs << " 条日志" << std::endl;
    std::cout << "成功发送: " << finalStats.successLogs << " 条日志" << std::endl;
    std::cout << "失败发送: " << finalStats.failedLogs << " 条日志" << std::endl;
    std::cout << "总耗时: " << std::fixed << std::setprecision(2) << finalStats.elapsedSeconds << " 秒" << std::endl;
    std::cout << "平均速率: " << std::fixed << std::setprecision(2) << finalStats.logsPerSecond << " 条/秒" << std::endl;
    
    // 目标速率的百分比
    double percentOfTarget = (finalStats.logsPerSecond / logsPerSecond) * 100.0;
    std::cout << "目标速率: " << logsPerSecond << " 条/秒" << std::endl;
    std::cout << "达到目标的: " << std::fixed << std::setprecision(2) << percentOfTarget << "%" << std::endl;
    
    // 生成报告
    if (!reportFile.empty()) {
        std::ofstream report(reportFile);
        if (report.is_open()) {
            // 写入CSV头
            report << "时间(秒),总日志数,成功数,失败数,每秒日志数\n";
            
            // 写入历史记录
            for (const auto& stats : statsHistory) {
                report << std::fixed << std::setprecision(2) << stats.elapsedSeconds << ","
                       << stats.totalLogs << ","
                       << stats.successLogs << ","
                       << stats.failedLogs << ","
                       << std::fixed << std::setprecision(2) << stats.logsPerSecond << "\n";
            }
            
            // 写入摘要
            report << "\n摘要\n";
            report << "总日志数," << finalStats.totalLogs << "\n";
            report << "成功发送," << finalStats.successLogs << "\n";
            report << "失败发送," << finalStats.failedLogs << "\n";
            report << "总耗时(秒)," << std::fixed << std::setprecision(2) << finalStats.elapsedSeconds << "\n";
            report << "平均速率(条/秒)," << std::fixed << std::setprecision(2) << finalStats.logsPerSecond << "\n";
            report << "目标速率(条/秒)," << logsPerSecond << "\n";
            report << "达到目标的(%)," << std::fixed << std::setprecision(2) << percentOfTarget << "\n";
            
            report.close();
            std::cout << "性能报告已写入: " << reportFile << std::endl;
        } else {
            std::cerr << "无法打开报告文件: " << reportFile << std::endl;
        }
    }
}

// 日志发送回调
void logSendCallback(bool success) {
    if (success) {
        successLogs++;
    } else {
        failedLogs++;
    }
    totalLogs++;
}

// 生成和发送日志的工作线程
void workerThread(int threadId, int logsPerThread, LogGeneratorConfig config, 
                 std::string targetHost, int targetPort) {
    
    // 创建TCP客户端
    TcpClient client("LogClient" + std::to_string(threadId), targetHost, targetPort, true);
    
    // 设置连接回调
    bool connected = false;
    std::condition_variable connCv;
    std::mutex connMutex;
    
    client.SetConnectionCallback([&](bool conn) {
        std::lock_guard<std::mutex> lock(connMutex);
        connected = conn;
        connCv.notify_all();
    });
    
    // 设置消息回调
    client.SetMessageCallback([](const std::string&, muduo::Timestamp) {
        // 忽略响应
    });
    
    // 连接服务器
    client.Connect();
    
    // 等待连接建立
    {
        std::unique_lock<std::mutex> lock(connMutex);
        if (!connCv.wait_for(lock, std::chrono::seconds(5), [&connected] { return connected; })) {
            std::cerr << "线程 " << threadId << " 连接服务器超时" << std::endl;
            return;
        }
    }
    
    // 创建日志生成器
    config.logsPerSecond = logsPerThread;
    LogGenerator generator(config);
    
    // 设置日志回调函数
    generator.SetLogCallback([&client](const std::string& logContent) {
        client.Send(logContent);
        logSendCallback(true);
    });
    
    // 开始生成日志
    generator.Start();
    
    // 等待生成器完成或被中断
    while (running && generator.GetGeneratedLogCount() < static_cast<uint64_t>(logsPerThread)) {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    // 停止生成器
    generator.Stop();
    generator.Wait();
    
    // 断开连接
    client.Disconnect();
}

int main(int argc, char* argv[]) {
    // 注册信号处理器
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // 默认参数
    int logsPerSecond = 1000;
    int durationSeconds = 60;
    int numThreads = 4;
    std::string targetHost = "127.0.0.1";
    int targetPort = 8000;
    int complexity = 5;
    bool outputToConsole = false;
    bool outputToFile = false;
    bool outputToNetwork = true;
    std::string reportFile = "benchmark_report.csv";
    
    // 解析命令行参数
    if (!parseCommandLine(argc, argv, logsPerSecond, durationSeconds, numThreads,
                          targetHost, targetPort, complexity,
                          outputToConsole, outputToFile, outputToNetwork, reportFile)) {
        return 1;
    }
    
    // 计算每个线程需要生成的日志数量
    int logsPerThread = logsPerSecond / numThreads;
    if (logsPerThread < 1) {
        logsPerThread = 1;
        numThreads = logsPerSecond;
    }
    
    // 显示配置信息
    std::cout << "===== 性能测试配置 =====" << std::endl;
    std::cout << "生成速率: " << logsPerSecond << " 条/秒" << std::endl;
    std::cout << "运行时间: " << durationSeconds << " 秒" << std::endl;
    std::cout << "线程数: " << numThreads << std::endl;
    std::cout << "每线程速率: " << logsPerThread << " 条/秒" << std::endl;
    std::cout << "目标服务器: " << targetHost << ":" << targetPort << std::endl;
    std::cout << "内容复杂度: " << complexity << std::endl;
    std::cout << "输出到控制台: " << (outputToConsole ? "是" : "否") << std::endl;
    std::cout << "输出到文件: " << (outputToFile ? "是" : "否") << std::endl;
    std::cout << "输出到网络: " << (outputToNetwork ? "是" : "否") << std::endl;
    std::cout << "报告文件: " << (reportFile.empty() ? "无" : reportFile) << std::endl;
    std::cout << "==========================" << std::endl;
    
    // 创建基本配置
    LogGeneratorConfig config;
    config.logsPerSecond = logsPerThread;
    config.duration = std::chrono::seconds(0);  // 无限时间，由主程序控制
    config.contentComplexity = complexity;
    config.outputToConsole = outputToConsole;
    config.outputToFile = outputToFile;
    config.outputToNetwork = outputToNetwork;
    config.targetHost = targetHost;
    config.targetPort = targetPort;
    
    // 调整批处理大小，使其适应生成率
    if (logsPerThread > 10000) {
        config.batchSize = 1000;  // 高速率使用大批次
    } else if (logsPerThread > 1000) {
        config.batchSize = 100;   // 中速率使用中批次
    } else {
        config.batchSize = 10;    // 低速率使用小批次
    }
    
    // 创建工作线程
    std::vector<std::thread> threads;
    std::cout << "启动 " << numThreads << " 个工作线程..." << std::endl;
    
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_back(workerThread, i, logsPerThread, config, targetHost, targetPort);
    }
    
    // 创建统计线程
    std::thread statsThreadObj(statsThread, logsPerSecond, durationSeconds, reportFile);
    
    // 等待所有线程完成
    for (auto& t : threads) {
        if (t.joinable()) {
            t.join();
        }
    }
    
    // 等待统计线程完成
    running = false;
    if (statsThreadObj.joinable()) {
        statsThreadObj.join();
    }
    
    return 0;
} 