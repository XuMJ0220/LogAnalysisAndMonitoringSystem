#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>
#include "xumj/network/tcp_server.h"

using namespace xumj::network;

std::atomic<bool> running(true);

void signalHandler(int sig) {
    std::cout << "收到信号: " << sig << std::endl;
    running = false;
}

int main() {
    // 注册信号处理器
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    std::cout << "创建TCP服务器..." << std::endl;
    TcpServer server("TestServer", "0.0.0.0", 8001, 4);
    
    // 设置连接回调
    server.SetConnectionCallback([](uint64_t connectionId, const std::string& clientAddr, bool connected) {
        if (connected) {
            std::cout << "新连接 [" << connectionId << "] 来自 " << clientAddr << std::endl;
        } else {
            std::cout << "连接断开 [" << connectionId << "] 来自 " << clientAddr << std::endl;
        }
    });
    
    // 设置消息回调
    server.SetMessageCallback([&server](uint64_t connectionId, const std::string& message, muduo::Timestamp) {
        std::cout << "接收到消息: 连接ID=" << connectionId 
                  << ", 大小=" << message.size() 
                  << ", 预览=" << (message.size() > 30 ? message.substr(0, 30) + "..." : message)
                  << std::endl;
        
        // 回显消息
        std::string response = "已收到消息: " + message;
        server.Send(connectionId, response);
    });
    
    std::cout << "启动TCP服务器..." << std::endl;
    server.Start();
    
    std::cout << "TCP服务器已启动，监听端口: 8001" << std::endl;
    std::cout << "按Ctrl+C停止..." << std::endl;
    
    // 主循环
    while (running) {
        std::this_thread::sleep_for(std::chrono::seconds(1));
        std::cout << "." << std::flush;
    }
    
    std::cout << std::endl << "停止TCP服务器..." << std::endl;
    server.Stop();
    std::cout << "TCP服务器已停止" << std::endl;
    
    return 0;
} 