#include <xumj/network/tcp_server.h>
#include <iostream>
#include <thread>
#include <chrono>
#include <signal.h>
#include <atomic>
#include <map>
#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <mutex>

std::atomic<bool> g_running(true);
std::mutex g_consoleMutex; // 添加控制台互斥锁，避免输出混乱

// 全局服务器指针，用于信号处理
xumj::network::TcpServer* g_server = nullptr;

void signalHandler(int sig) {
    std::lock_guard<std::mutex> lock(g_consoleMutex);
    std::cout << "\n收到信号: " << sig << "，准备退出..." << std::endl;
    g_running = false;
}

// 帮助信息
void printHelp() {
    std::lock_guard<std::mutex> lock(g_consoleMutex);
    std::cout << "\n可用命令：" << std::endl
              << "  help     - 显示此帮助信息" << std::endl
              << "  list     - 列出所有连接" << std::endl
              << "  send     - 发送消息到指定连接 (用法: send <连接ID> <消息>)" << std::endl
              << "  broadcast- 广播消息到所有连接 (用法: broadcast <消息>)" << std::endl
              << "  kick     - 断开指定连接 (用法: kick <连接ID>)" << std::endl
              << "  stats    - 显示服务器统计信息" << std::endl
              << "  quit     - 退出服务器" << std::endl;
}

// 处理用户输入的命令
void handleUserCommand(xumj::network::TcpServer& server, const std::string& cmd) {
    std::istringstream iss(cmd);
    std::string command;
    iss >> command;

    std::lock_guard<std::mutex> lock(g_consoleMutex);
    if (command == "help") {
        printHelp();
    }
    else if (command == "list") {
        std::cout << "当前连接数: " << server.GetConnectionCount() << std::endl;
    }
    else if (command == "send") {
        uint64_t connId;
        std::string message;
        if (iss >> connId) {
            std::getline(iss, message);
            if (!message.empty()) {
                message = message.substr(1); // 移除前导空格
                try {
                    if (server.Send(connId, message)) {
                        std::cout << "消息已发送到连接 " << connId << std::endl;
                    } else {
                        std::cout << "发送失败：连接 " << connId << " 不存在或已断开" << std::endl;
                    }
                } catch (const std::exception& e) {
                    std::cerr << "发送消息时发生异常: " << e.what() << std::endl;
                }
            } else {
                std::cout << "错误：消息不能为空" << std::endl;
            }
        } else {
            std::cout << "用法: send <连接ID> <消息>" << std::endl;
        }
    }
    else if (command == "broadcast") {
        std::string message;
        std::getline(iss, message);
        if (!message.empty()) {
            message = message.substr(1); // 移除前导空格
            try {
                size_t count = server.Broadcast(message);
                std::cout << "消息已广播到 " << count << " 个连接" << std::endl;
            } catch (const std::exception& e) {
                std::cerr << "广播消息时发生异常: " << e.what() << std::endl;
            }
        } else {
            std::cout << "错误：消息不能为空" << std::endl;
        }
    }
    else if (command == "kick") {
        uint64_t connId;
        if (iss >> connId) {
            try {
                if (server.CloseConnection(connId)) {
                    std::cout << "已断开连接 " << connId << std::endl;
                } else {
                    std::cout << "断开失败：连接 " << connId << " 不存在" << std::endl;
                }
            } catch (const std::exception& e) {
                std::cerr << "断开连接时发生异常: " << e.what() << std::endl;
            }
        } else {
            std::cout << "用法: kick <连接ID>" << std::endl;
        }
    }
    else if (command == "stats") {
        std::cout << "\n服务器统计信息：" << std::endl
                  << "- 名称: " << server.GetServerName() << std::endl
                  << "- 地址: " << server.GetListenAddr() << ":" << server.GetPort() << std::endl
                  << "- 工作线程数: " << server.GetNumThreads() << std::endl
                  << "- 当前连接数: " << server.GetConnectionCount() << std::endl;
    }
    else if (command == "quit") {
        g_running = false;
    }
    else if (!command.empty()) {
        std::cout << "未知命令: " << command << std::endl;
        std::cout << "输入 'help' 查看可用命令" << std::endl;
    }
}

int main() {
    try {
        // 注册信号处理
        signal(SIGINT, signalHandler);
        signal(SIGTERM, signalHandler);
        signal(SIGSEGV, signalHandler);
        
        {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cout << "启动TCP服务器示例程序..." << std::endl;
        }
        
        // 创建TCP服务器
        xumj::network::TcpServer server("ExampleServer", "0.0.0.0", 9876);
        g_server = &server;
        
        // 设置连接回调
        server.SetConnectionCallback([](uint64_t connId, const std::string& clientAddr, bool connected) {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cout << "\n===== 连接事件 =====" << std::endl
                     << "- 状态: " << (connected ? "新连接" : "连接断开") << std::endl
                     << "- 连接ID: " << connId << std::endl
                     << "- 客户端: " << clientAddr << std::endl;
            
            // 如果是新连接，发送欢迎消息
            if (connected && g_server) {
                try {
                    std::ostringstream oss;
                    oss << "欢迎连接到TCP服务器示例程序！\n"
                        << "您的连接ID是: " << connId << "\n";
                    
                    // 获取当前时间并格式化
                    std::time_t now = std::time(nullptr);
                    oss << "当前时间: " << std::put_time(std::localtime(&now), "%Y-%m-%d %H:%M:%S");
                    
                    // 延迟一点发送欢迎消息
                    std::this_thread::sleep_for(std::chrono::milliseconds(100));
                    g_server->Send(connId, oss.str());
                } catch (const std::exception& e) {
                    std::cerr << "发送欢迎消息时发生异常: " << e.what() << std::endl;
                } catch (...) {
                    std::cerr << "发送欢迎消息时发生未知异常" << std::endl;
                }
            }
        });
        
        // 设置消息回调
        server.SetMessageCallback([&server](uint64_t connId, const std::string& message, muduo::Timestamp time) {
            try {
                std::lock_guard<std::mutex> lock(g_consoleMutex);
                std::cout << "\n===== 收到消息 =====" << std::endl
                          << "- 连接ID: " << connId << std::endl
                          << "- 时间: " << time.toFormattedString() << std::endl
                          << "- 内容: [" << message << "]" << std::endl;
                
                // 构建响应消息
                std::ostringstream oss;
                oss << "服务器已收到您的消息：\n"
                    << "- 消息长度：" << message.length() << " 字节\n"
                    << "- 接收时间：" << time.toFormattedString() << "\n"
                    << "- 原始内容：" << message;
                
                // 发送响应
                server.Send(connId, oss.str());
            } catch (const std::exception& e) {
                std::cerr << "处理消息时发生异常: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "处理消息时发生未知异常" << std::endl;
            }
        });
        
        // 启动服务器
        {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cout << "正在启动服务器..." << std::endl;
        }
        
        // Start()方法返回void，不能用于条件判断
        server.Start();
        
        // 启动后检查服务器状态
        if (server.IsRunning()) {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cout << "服务器已启动，监听地址: " << server.GetListenAddr() << ":" << server.GetPort() << std::endl;
            
            // 显示帮助信息
            printHelp();
        } else {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cerr << "服务器启动失败!" << std::endl;
            return 1;
        }
        
        // 主循环 - 处理用户输入
        std::string input;
        while (g_running) {
            {
                std::lock_guard<std::mutex> lock(g_consoleMutex);
                std::cout << "\n> ";
            }
            std::getline(std::cin, input);
            
            if (!g_running) break;
            
            handleUserCommand(server, input);
        }
        
        // 停止服务器
        {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cout << "\n正在停止服务器..." << std::endl;
        }
        
        g_server = nullptr;
        server.Stop();
        
        {
            std::lock_guard<std::mutex> lock(g_consoleMutex);
            std::cout << "服务器已停止." << std::endl;
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