#include <iostream>
#include <string>
#include <thread>
#include <chrono>
#include <atomic>
#include <signal.h>
#include "xumj/network/tcp_client.h"

using namespace xumj::network;

std::atomic<bool> running(true);

void signalHandler(int sig) {
    std::cout << "收到信号: " << sig << std::endl;
    running = false;
}

int main(int argc, char* argv[]) {
    // 注册信号处理器
    signal(SIGINT, signalHandler);
    signal(SIGTERM, signalHandler);
    
    // 解析参数
    std::string host = "127.0.0.1";
    int port = 8001;
    
    if (argc > 1) {
        host = argv[1];
    }
    
    if (argc > 2) {
        port = std::stoi(argv[2]);
    }
    
    std::cout << "创建TCP客户端，连接到 " << host << ":" << port << "..." << std::endl;
    TcpClient client("TestClient", host, port, true);
    
    // 设置连接回调
    client.SetConnectionCallback([](bool connected) {
        if (connected) {
            std::cout << "已连接到服务器" << std::endl;
        } else {
            std::cout << "与服务器断开连接" << std::endl;
        }
    });
    
    // 设置消息回调
    client.SetMessageCallback([](const std::string& message, muduo::Timestamp) {
        std::cout << "收到服务器响应: " << message << std::endl;
    });
    
    // 连接服务器
    client.Connect();
    
    // 等待连接建立
    for (int i = 0; i < 30 && running; ++i) {
        if (client.IsConnected()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    
    if (!client.IsConnected()) {
        std::cerr << "无法连接到服务器，退出" << std::endl;
        return 1;
    }
    
    std::cout << "已连接到服务器，开始发送消息" << std::endl;
    std::cout << "每秒发送一条消息，按Ctrl+C停止..." << std::endl;
    
    int messageId = 0;
    while (running) {
        // 发送消息
        std::string message = "测试消息 #" + std::to_string(++messageId);
        std::cout << "发送: " << message << std::endl;
        client.Send(message);
        
        // 等待1秒
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    
    std::cout << "断开连接..." << std::endl;
    client.Disconnect();
    std::cout << "客户端已停止" << std::endl;
    
    return 0;
} 