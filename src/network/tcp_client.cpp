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
        return true;  // 已经连接
    }
    
    // 使用promise/future来确保在返回前连接已经启动
    std::promise<void> initPromise;
    std::future<void> initFuture = initPromise.get_future();
    
    // 启动事件循环线程，在线程内创建和使用EventLoop
    loopThread_ = std::thread([this, &initPromise]() {
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
        
        // 设置连接回调
        client_->setConnectionCallback(
            [this](const TcpConnectionPtr& conn) {
                HandleConnection(conn);
            }
        );
        
        // 设置消息回调
        client_->setMessageCallback(
            [this](const TcpConnectionPtr& conn,
                  muduo::net::Buffer* buffer,
                  muduo::Timestamp timestamp) {
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
        
        // 运行事件循环，这会阻塞当前线程直到loop_.quit()被调用
        loop.loop();
        
        // 事件循环结束后，清除指针
        loop_ = nullptr;
        client_ = nullptr;
        
        std::cout << "TCP Client [" << clientName_ << "] event loop stopped." << std::endl;
    });
    
    // 等待初始化完成
    initFuture.wait();
    
    return true;
}

void TcpClient::Disconnect() {
    if (!IsConnected() && !IsReconnecting()) {
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
        loop_->runInLoop([this]() {
            if (client_) {
                client_->disconnect();
            }
            loop_->quit();
        });
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
    auto conn = GetConnection();
    if (conn && conn->connected()) {
        conn->send(message);
        return true;
    }
    
    return false;
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

void TcpClient::HandleConnection(const TcpConnectionPtr& conn) {
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
        connectionCallback_(connected);
    }
}

void TcpClient::HandleMessage(const TcpConnectionPtr& /* conn */, 
                             muduo::net::Buffer* buffer,
                             muduo::Timestamp timestamp) {
    // 获取消息内容
    std::string message = buffer->retrieveAllAsString();
    
    // 调用用户回调
    if (messageCallback_) {
        messageCallback_(message, timestamp);
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

TcpClient::TcpConnectionPtr TcpClient::GetConnection() {
    std::lock_guard<std::mutex> lock(connectionMutex_);
    return connection_;
}

} // namespace network
} // namespace xumj 