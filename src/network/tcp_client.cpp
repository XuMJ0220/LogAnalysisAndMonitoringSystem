#include "xumj/network/tcp_client.h"
#include <iostream>
#include <thread>
#include <future>

namespace xumj {
namespace network {

TcpClient::TcpClient(const std::string& clientName,
                     const std::string& serverAddr,
                     uint16_t port,
                     bool autoReconnect)
    : clientName_(clientName),
      serverAddr_(serverAddr),
      serverPort_(port),
      autoReconnect_(autoReconnect),
      connected_(false),
      reconnecting_(false),
      loop_(nullptr),
      client_(nullptr) {
    
    std::cout << "TCP Client [" << clientName_ << "] created for server " 
              << serverAddr_ << ":" << serverPort_ << std::endl;
}

TcpClient::~TcpClient() {
    Disconnect();
}

bool TcpClient::Connect() {
    if (IsConnected()) {
        std::cout << "INFO: 客户端已经连接" << std::endl;
        return true;  // 已经连接
    }
    
    // 使用promise/future来确保在返回前连接已经启动
    std::promise<void> initPromise;
    std::future<void> initFuture = initPromise.get_future();
    
    // 启动事件循环线程，在线程内创建和使用EventLoop
    loopThread_ = std::thread([this, &initPromise]() {
        try {
            // 在事件循环线程中创建EventLoop
            muduo::net::EventLoop loop;
            
            // 创建服务器地址
            muduo::net::InetAddress serverAddress(serverAddr_, serverPort_);
            
            // 创建TCP客户端
            muduo::net::TcpClient client(&loop, serverAddress, clientName_);
            
            // 保存指针（线程安全，因为此时其他线程还在等待initPromise）
            loop_ = &loop;
            client_ = &client;
            
            // 设置自动重连
            if (autoReconnect_) {
                client_->enableRetry();
            }
            
            // 使用Lambda表达式设置连接回调
            client_->setConnectionCallback(
                [this](const muduo::net::TcpConnectionPtr& conn) {
                    std::cout << "!!!!! 直接输出: 连接回调被触发: " 
                            << (conn->connected() ? "已连接" : "已断开") 
                            << ", 到: " << conn->peerAddress().toIpPort() 
                            << std::endl;
                    
                    HandleConnection(conn);
                }
            );
            
            // 使用Lambda表达式设置消息回调
            client_->setMessageCallback(
                [this](const muduo::net::TcpConnectionPtr& conn,
                      muduo::net::Buffer* buffer,
                      muduo::Timestamp timestamp) {
                    std::cout << "!!!!! 直接输出: 消息回调被触发, 来自: " 
                            << conn->peerAddress().toIpPort() 
                            << ", 消息大小: " << buffer->readableBytes() 
                            << " 字节" << std::endl;
                    
                    HandleMessage(conn, buffer, timestamp);
                }
            );
            
            // 启动连接
            client_->connect();
            SetReconnecting(true);
            
            std::cout << "TCP Client [" << clientName_ << "] connecting to " 
                      << serverAddr_ << ":" << serverPort_ << std::endl;
            
            // 通知主线程初始化完成
            initPromise.set_value();
            
            // 运行事件循环，这会阻塞当前线程直到loop.quit()被调用
            loop.loop();
            
            // 事件循环结束后，清除指针
            {
                std::lock_guard<std::mutex> lock(connectionMutex_);
                loop_ = nullptr;
                client_ = nullptr;
                connection_.reset();
            }
            
            // 更新状态
            SetConnected(false);
            SetReconnecting(false);
            
            std::cout << "TCP Client [" << clientName_ << "] event loop stopped." << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "异常: 客户端事件循环线程执行失败: " << e.what() << std::endl;
            
            loop_ = nullptr;
            client_ = nullptr;
            SetConnected(false);
            SetReconnecting(false);
            
            // 尽管出现异常，也要通知主线程初始化完成
            initPromise.set_value();
        }
    });
    
    // 等待初始化完成
    initFuture.wait();
    
    return true;
}

void TcpClient::Disconnect() {
    bool needStop = false;
    
    {
        std::lock_guard<std::mutex> lock(stateMutex_);
        needStop = connected_ || reconnecting_;
    }
    
    if (!needStop) {
        std::cout << "INFO: 客户端已经断开连接" << std::endl;
        return;  // 已经断开
    }
    
    // 更新状态
    SetConnected(false);
    SetReconnecting(false);
    autoReconnect_ = false;
    
    // 清空连接
    {
        std::lock_guard<std::mutex> lock(connectionMutex_);
        connection_.reset();
    }
    
    // 停止事件循环
    if (loop_) {
        try {
            loop_->runInLoop([this]() {
                if (client_) {
                    client_->disconnect();
                }
                loop_->quit();
            });
        }
        catch (const std::exception& e) {
            std::cerr << "异常: 停止客户端时发生错误: " << e.what() << std::endl;
        }
    }
    
    // 等待事件循环线程结束
    if (loopThread_.joinable()) {
        loopThread_.join();
    }
    
    std::cout << "TCP Client [" << clientName_ << "] disconnected from " 
              << serverAddr_ << ":" << serverPort_ << std::endl;
}

void TcpClient::SetMessageCallback(const MessageCallback& callback) {
    messageCallback_ = callback;
}

void TcpClient::SetConnectionCallback(const ConnectionCallback& callback) {
    connectionCallback_ = callback;
}

bool TcpClient::Send(const std::string& message) {
    return Send(message, false);
}

bool TcpClient::Send(const std::string& message, bool flushImmediately) {
    muduo::net::TcpConnectionPtr conn = GetConnection();
    if (!conn || !conn->connected()) {
        std::cout << "DEBUG: Send - 发送失败: " 
                << (conn ? "连接未处于connected状态" : "无连接") << std::endl;
        return false;
    }
    
    // 在消息末尾添加CRLF (\r\n)结束符
    std::string messageWithCRLF = message + "\r\n";
    
    try {
        // 获取连接所属的事件循环
        auto connLoop = conn->getLoop();
        
        if (flushImmediately) {
            std::cout << "DEBUG: Send(flush) - 发送消息: [" << message << "]，字节数: " 
                    << messageWithCRLF.size() << "，立即刷新: 是" << std::endl;
            
            // 在事件循环线程中执行发送，确保线程安全
            connLoop->runInLoop([conn, messageWithCRLF]() {
                conn->send(messageWithCRLF);
                
                // 立即发送一个小数据包强制刷新
                conn->send("");
            });
        } else {
            std::cout << "DEBUG: Send - 发送消息: [" << message << "]，字节数: " 
                    << messageWithCRLF.size() << std::endl;
            
            // 在事件循环线程中执行发送，确保线程安全
            connLoop->runInLoop([conn, messageWithCRLF]() {
                conn->send(messageWithCRLF);
            });
        }
        
        return true;
    }
    catch (const std::exception& e) {
        std::cerr << "异常: 发送消息时发生错误: " << e.what() << std::endl;
        return false;
    }
}

bool TcpClient::IsConnected() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return connected_;
}

bool TcpClient::IsReconnecting() const {
    std::lock_guard<std::mutex> lock(stateMutex_);
    return reconnecting_;
}

const std::string& TcpClient::GetClientName() const {
    return clientName_;
}

const std::string& TcpClient::GetServerAddr() const {
    return serverAddr_;
}

uint16_t TcpClient::GetServerPort() const {
    return serverPort_;
}

void TcpClient::HandleConnection(const muduo::net::TcpConnectionPtr& conn) {
    bool connected = conn->connected();
    
    // 更新连接状态
    SetConnected(connected);
    
    if (connected) {
        // 连接建立，保存连接对象
        {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            connection_ = conn;
        }
        
        // 连接成功，不再是重连状态
        SetReconnecting(false);
        
        std::cout << "TCP Client [" << clientName_ << "] connected to " 
                  << serverAddr_ << ":" << serverPort_ << std::endl;
    } else {
        // 连接断开，清空连接对象
        {
            std::lock_guard<std::mutex> lock(connectionMutex_);
            if (connection_ == conn) {
                connection_.reset();
            }
        }
        
        // 如果启用了自动重连，则设置重连状态
        if (autoReconnect_) {
            SetReconnecting(true);
        }
        
        std::cout << "TCP Client [" << clientName_ << "] disconnected from " 
                  << serverAddr_ << ":" << serverPort_ << std::endl;
    }
    
    // 调用用户回调
    if (connectionCallback_) {
        try {
            connectionCallback_(connected);
        }
        catch (const std::exception& e) {
            std::cerr << "异常: 执行连接回调时发生错误: " << e.what() << std::endl;
        }
    }
}

void TcpClient::HandleMessage(const muduo::net::TcpConnectionPtr& /* conn */, 
                             muduo::net::Buffer* buffer,
                             muduo::Timestamp timestamp) {
    // 打印调试信息
    std::cout << "客户端收到数据，总大小: " << buffer->readableBytes() << " 字节" << std::endl;
    
    const char* data = buffer->peek();
    size_t len = buffer->readableBytes();
    
    // 打印十六进制表示（最多100字节）
    std::cout << "收到原始数据十六进制: ";
    for (size_t i = 0; i < std::min(len, size_t(100)); ++i) {
        printf("%02X ", static_cast<unsigned char>(data[i]));
    }
    if (len > 100) {
        std::cout << "... (总共 " << len << " 字节)";
    }
    std::cout << std::endl;
    
    // 打印可读字符表示（最多1024字节）
    std::string readableData = len <= 1024 ? 
        std::string(data, len) : 
        std::string(data, 1024) + "... (总共 " + std::to_string(len) + " 字节)";
    std::cout << "收到原始数据字符表示: [" << readableData << "]" << std::endl;
    
    // 处理所有完整的消息
    while (buffer->readableBytes() > 0) {
        // 尝试查找消息结束符
        const char* crlf = buffer->findCRLF();
        if (crlf) {
            // 找到结束符，提取一行
            std::string message(buffer->peek(), crlf - buffer->peek());
            std::cout << "找到CRLF，提取消息: [" << message << "]" << std::endl;
            buffer->retrieveUntil(crlf + 2);  // 跳过CRLF
            
            // 调用用户回调
            if (messageCallback_) {
                try {
                    std::cout << "调用用户回调处理消息: [" << message << "]" << std::endl;
                    messageCallback_(message, timestamp);
                }
                catch (const std::exception& e) {
                    std::cerr << "异常: 执行消息回调时发生错误: " << e.what() << std::endl;
                }
            } else {
                std::cerr << "未设置消息回调函数，无法处理接收到的消息" << std::endl;
            }
        } else {
            // 没有找到结束符，可能是消息不完整
            std::cerr << "未找到消息结束符，等待更多数据..." << std::endl;
            break;  // 退出循环，等待更多数据
        }
    }
}

void TcpClient::SetConnected(bool connected) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    connected_ = connected;
}

void TcpClient::SetReconnecting(bool reconnecting) {
    std::lock_guard<std::mutex> lock(stateMutex_);
    reconnecting_ = reconnecting;
}

muduo::net::TcpConnectionPtr TcpClient::GetConnection() {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    return connection_;
}

} // namespace network
} // namespace xumj 