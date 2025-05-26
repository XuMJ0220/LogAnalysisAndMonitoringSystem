/**
 * TCP客户端示例程序
 * 
 * 该示例演示如何使用xumj::network::TcpClient创建一个简单的TCP客户端。
 * 该客户端会定期向服务器发送消息，并打印接收到的所有响应。
 */

#include <xumj/network/tcp_client.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <signal.h>
#include <atomic>
#include <sstream>
#include <iomanip>
#include <mutex>
#include <ctime>
#include <stdexcept>

std::atomic<bool> g_running(true);
std::mutex g_consoleMutex;

// 全局客户端指针，用于信号处理
xumj::network::TcpClient* g_client = nullptr;

void signalHandler(int sig) {
    std::lock_guard<std::mutex> lock(g_consoleMutex);
    std::cout << "收到信号: " << sig << "，准备退出..." << std::endl;
    g_running = false;
}

// 帮助信息
void printHelp() {
    std::lock_guard<std::mutex> lock(g_consoleMutex);
    std::cout << "\n可用命令：" << std::endl
              << "  help     - 显示此帮助信息" << std::endl
              << "  send     - 发送消息到服务器 (用法: send <消息>)" << std::endl
              << "  status   - 显示连接状态" << std::endl
              << "  reconnect- 重新连接到服务器" << std::endl
              << "  quit     - 退出客户端" << std::endl;
}

// 处理用户输入的命令
void handleUserCommand(xumj::network::TcpClient& client, const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string command;
    iss >> command;

    std::lock_guard<std::mutex> lock(g_consoleMutex);
    if (command == "help") {
        printHelp();
    }
    else if (command == "send") {
        std::string message;
        std::getline(iss, message);
        if (!message.empty()) {
            message = message.substr(1); // 移除前导空格
            try {
                if (client.Send(message)) {
                    std::cout << "消息已发送" << std::endl;
                } else {
                    std::cout << "发送失败：未连接到服务器" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "发送消息时发生异常: " << e.what() << std::endl;
            }
        } else {
            std::cout << "错误：消息不能为空" << std::endl;
        }
    }
    else if (command == "status") {
        try {
            std::cout << "\n连接状态：" << std::endl
                      << "- 服务器: " << client.GetServerAddr() << ":" << client.GetServerPort() << std::endl
                      << "- 状态: " << (client.IsConnected() ? "已连接" : "未连接") << std::endl;
        } catch (const std::exception& e) {
            std::cerr << "获取状态时发生异常: " << e.what() << std::endl;
        }
    }
    else if (command == "reconnect") {
        try {
            if (client.IsConnected()) {
                std::cout << "已经连接到服务器" << std::endl;
            } else {
                std::cout << "正在重新连接..." << std::endl;
                if (client.Connect()) {
                    std::cout << "重新连接成功" << std::endl;
                } else {
                    std::cout << "重新连接失败" << std::endl;
                }
            }
        } catch (const std::exception& e) {
            std::cerr << "重新连接时发生异常: " << e.what() << std::endl;
        }
    }
    else if (command == "quit") {
        g_running = false;
    }
    else if (!command.empty()) {
        std::cout << "未知命令: " << command << std::endl;
        std::cout << "输入 'help' 查看可用命令" << std::endl;
    }
}

int main(int argc, char* argv[]) {
    try {
        // 注册信号处理
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        signal(SIGSEGV, signalHandler);
        
        {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cout << "启动TCP客户端示例程序..." << std::endl;
        }
        
        // 默认连接参数
        std::string serverAddr = "127.0.0.1";
        uint16_t serverPort = 9876;
        
        // 解析命令行参数
        if (argc > 1) serverAddr = argv[1];
        if (argc > 2) {
            try {
                serverPort = static_cast<uint16_t>(std::stoi(argv[2]));
            } catch (const std::exception& e) {
                std::cerr << "端口参数错误: " << e.what() << std::endl;
                std::cerr << "使用默认端口: " << serverPort << std::endl;
            }
        }
        
        // 创建客户端
        xumj::network::TcpClient client("ExampleClient", serverAddr, serverPort, true);
        g_client = &client;
        
        // 设置连接回调
        client.SetConnectionCallback([](bool connected) {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::time_t now = std::time(nullptr);
            std::cout << "\n===== 连接状态变化 =====" << std::endl
                      << "- 状态: " << (connected ? "已连接到服务器" : "已断开连接") << std::endl
                      << "- 时间: " << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S") << std::endl;
        });
        
        // 设置消息回调
        client.SetMessageCallback([](const std::string& message, muduo::Timestamp time) {
            try {
                std::lock_guard<std::mutex> lock(g_consoleMutex);
                if (message.empty()) {
                    std::cout << "\n警告: 收到空消息" << std::endl;
                    return;
                }
                
                std::cout << "\n===== 收到服务器消息 =====" << std::endl
                          << "- 时间: " << time.toFormattedString() << std::endl
                          << "- 内容:\n" << message << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "处理消息回调时发生异常: " << e.what() << std::endl;
            }
        });
        
        // 连接到服务器
        {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cout << "正在连接到服务器: " << serverAddr << ":" << serverPort << "..." << std::endl;
        }
        
        if (!client.Connect()) {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cerr << "连接服务器失败!" << std::endl;
            return 1;
        }
        
        // 等待连接建立
        {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cout << "等待连接建立..." << std::endl;
        }
        
        bool connected = false;
        for (int i = 0; i < 5; ++i) {
            if (client.IsConnected()) {
                connected = true;
                std::lock_guard<std::mutex> lock(g_consoleMutex);
                std::cout << "已成功连接到服务器!" << std::endl;
                break;
            }
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        
        if (!connected) {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cerr << "连接超时，退出程序!" << std::endl;
            return 1;
        }
        
        // 显示帮助信息
        printHelp();
        
        // 主循环 - 处理用户输入
        std::string input;
        while (g_running) {
            {
                std::lock_guard<std::mutex> lock(g_consoleMutex);
                std::cout << "\n> ";
            }
            std::getline(std::cin, input);
            
            if (!g_running) break;
            
            handleUserCommand(client, input);
        }
        
        // 断开连接
        {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cout << "\n正在断开连接..." << std::endl;
        }
        
        g_client = nullptr;
        client.Disconnect();
        
        {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cout << "已断开连接，程序退出." << std::endl;
        }
    } catch (const std::exception& e) {
        std::lock_guard<std::mutex> lock(g_consoleMutex);
        std::cerr << "致命错误: " << e.what() << std::endl;
        return 1;
    } catch (...) {
        std::lock_guard<std::mutex> lock(g_consoleMutex);
        std::cerr << "未知致命错误" << std::endl;
        return 1;
    }
    
    return 0;
}