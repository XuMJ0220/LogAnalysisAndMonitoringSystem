#include "xumj/network/tcp_server.h"
#include "xumj/network/tcp_client.h"
#include <iostream>
#include <thread>
#include <chrono>
#include <string>
#include <atomic>
#include <csignal>
#include <mutex>
#include <condition_variable>

std::atomic<bool> running(true);
std::mutex connectionMutex;
std::condition_variable connectionCV;
bool clientConnected = false;

// 信号处理函数，用于优雅退出
void signalHandler(int signal) {
    std::cout << "收到信号: " << signal << std::endl;
    running = false;
}

// 服务器示例
void runServer() {
    // 创建TCP服务器
    xumj::network::TcpServer server("LogServer", "0.0.0.0", 8888, 4);
    
    // 设置连接回调
    server.SetConnectionCallback([](uint64_t connectionId, const std::string& clientAddr, bool connected) {
        if (connected) {
            std::cout << "服务器: 新连接 [" << connectionId << "] 来自 " << clientAddr << std::endl;
        } else {
            std::cout << "服务器: 连接断开 [" << connectionId << "] 来自 " << clientAddr << std::endl;
        }
    });
    
    // 设置消息回调
    server.SetMessageCallback([&server](uint64_t connectionId, const std::string& message, muduo::Timestamp /* timestamp */) {
        std::cout << "服务器: 收到消息 [" << connectionId << "]: " << message << std::endl;
        
        // 回复客户端
        std::string reply = "服务器已收到: " + message;
        server.Send(connectionId, reply);
    });
    
    // 启动服务器
    server.Start();
    
    std::cout << "服务器已启动，按Ctrl+C退出..." << std::endl;
    
    // 主循环
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "正在停止服务器..." << std::endl;
    server.Stop();
}

// 客户端示例
void runClient() {
    // 等待服务器启动
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 创建TCP客户端
    xumj::network::TcpClient client("LogClient", "127.0.0.1", 8888);
    
    // 设置连接回调
    client.SetConnectionCallback([](bool connected) {
        if (connected) {
            std::cout << "客户端: 已连接到服务器" << std::endl;
            std::lock_guard<std::mutex> lock(connectionMutex);
            clientConnected = true;
            connectionCV.notify_one();
        } else {
            std::cout << "客户端: 已与服务器断开连接" << std::endl;
            std::lock_guard<std::mutex> lock(connectionMutex);
            clientConnected = false;
        }
    });
    
    // 设置消息回调
    client.SetMessageCallback([](const std::string& message, muduo::Timestamp /* timestamp */) {
        std::cout << "客户端: 收到消息: " << message << std::endl;
    });
    
    // 连接到服务器
    client.Connect();
    
    // 等待连接建立或超时
    {
        std::unique_lock<std::mutex> lock(connectionMutex);
        if (!clientConnected) {
            connectionCV.wait_for(lock, std::chrono::seconds(5), []{ return clientConnected; });
        }
    }
    
    if (!client.IsConnected()) {
        std::cout << "客户端: 连接服务器失败" << std::endl;
        return;
    }
    
    // 发送消息
    int messageCount = 0;
    while (running && messageCount < 10 && client.IsConnected()) {
        std::string message = "测试消息 #" + std::to_string(++messageCount);
        
        if (client.Send(message)) {
            std::cout << "客户端: 发送消息: " << message << std::endl;
        } else {
            std::cout << "客户端: 发送消息失败" << std::endl;
        }
        
        // 等待一段时间
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "客户端: 正在断开连接..." << std::endl;
    client.Disconnect();
    
    // 客户端的事件循环在这里运行，确保在创建EventLoop的线程中使用它
    if (running) {
        std::cout << "客户端: 事件循环开始运行..." << std::endl;
        // 不要调用loop()，因为客户端内部已经在一个分离的线程中运行了事件循环
    }
}

int main() {
    // 注册信号处理函数
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    
    // 启动服务器线程
    std::thread serverThread(runServer);
    
    // 启动客户端线程
    std::thread clientThread(runClient);
    
    // 等待线程结束
    if (serverThread.joinable()) {
        serverThread.join();
    }
    
    if (clientThread.joinable()) {
        clientThread.join();
    }
    
    std::cout << "示例运行完成" << std::endl;
    return 0;
} 