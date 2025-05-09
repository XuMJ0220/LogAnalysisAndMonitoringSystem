#include "xumj/network/tcp_server.h"
#include <iostream>
#include <sstream>
#include <mutex>

namespace xumj {
namespace network {

TcpServer::TcpServer(const std::string& serverName,
                     const std::string& listenAddr,
                     uint16_t port,
                     size_t numThreads)
    : serverName_(serverName),
      listenAddr_(listenAddr),
      port_(port),
      numThreads_(numThreads),
      running_(false),
      nextConnectionId_(1) {
    
    // 创建事件循环
    loop_ = std::make_unique<muduo::net::EventLoop>();
    
    // 创建服务器地址
    muduo::net::InetAddress serverAddr(listenAddr_, port_);
    
    // 创建TCP服务器
    server_ = std::make_unique<muduo::net::TcpServer>(loop_.get(), serverAddr, serverName_);
    
    // 设置线程数量
    if (numThreads_ == 0) {
        numThreads_ = std::thread::hardware_concurrency();
    }
    server_->setThreadNum(numThreads_);
    
    // 设置回调函数
    server_->setConnectionCallback(
        [this](const muduo::net::TcpConnectionPtr& conn) {
            HandleConnection(conn, conn->connected());
        }
    );
    
    server_->setMessageCallback(
        [this](const muduo::net::TcpConnectionPtr& conn,
               muduo::net::Buffer* buffer,
               muduo::Timestamp timestamp) {
            HandleMessage(conn, buffer, timestamp);
        }
    );
    
    std::cout << "TCP Server [" << serverName_ << "] created at " 
              << listenAddr_ << ":" << port_ << " with " 
              << numThreads_ << " threads." << std::endl;
}

TcpServer::~TcpServer() {
    Stop();
}

void TcpServer::Start() {
    if (!running_) {
        server_->start();
        running_ = true;
        std::cout << "TCP Server [" << serverName_ << "] started." << std::endl;
    }
}

void TcpServer::Stop() {
    if (running_) {
        // 标记服务器为非运行状态
        running_ = false;
        
        // 清理所有连接
        {
            std::lock_guard<std::mutex> lock(connectionsMutex_);
            connections_.clear();
        }
        
        // 停止服务器
        // 注意：muduo的TcpServer没有直接的stop方法，它会在EventLoop停止时自动停止
        if (loop_) {
            loop_->quit();
        }
        
        std::cout << "TCP Server [" << serverName_ << "] stopped." << std::endl;
    }
}

void TcpServer::SetMessageCallback(const MessageCallback& callback) {
    messageCallback_ = callback;
}

void TcpServer::SetConnectionCallback(const ConnectionCallback& callback) {
    connectionCallback_ = callback;
}

bool TcpServer::Send(uint64_t connectionId, const std::string& message) {
    auto conn = GetConnection(connectionId);
    if (conn && conn->connected()) {
        conn->send(message);
        return true;
    }
    return false;
}

size_t TcpServer::Broadcast(const std::string& message) {
    size_t successCount = 0;
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    
    for (const auto& pair : connections_) {
        if (pair.second && pair.second->connected()) {
            pair.second->send(message);
            ++successCount;
        }
    }
    
    return successCount;
}

bool TcpServer::CloseConnection(uint64_t connectionId) {
    auto conn = GetConnection(connectionId);
    if (conn) {
        conn->shutdown();
        return true;
    }
    return false;
}

size_t TcpServer::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return connections_.size();
}

bool TcpServer::IsRunning() const {
    return running_;
}

const std::string& TcpServer::GetServerName() const {
    return serverName_;
}

const std::string& TcpServer::GetListenAddr() const {
    return listenAddr_;
}

uint16_t TcpServer::GetPort() const {
    return port_;
}

void TcpServer::HandleConnection(const TcpConnectionPtr& conn, bool connected) {
    // 获取客户端地址
    std::string clientAddr = conn->peerAddress().toIpPort();
    
    if (connected) {
        // 新连接建立
        uint64_t connectionId = GenerateConnectionId();
        
        // 在连接上存储ID
        conn->setContext(connectionId);
        
        // 注册连接
        RegisterConnection(connectionId, conn);
        
        // 调用用户回调
        if (connectionCallback_) {
            connectionCallback_(connectionId, clientAddr, true);
        }
        
        std::cout << "New connection [" << connectionId << "] from " 
                  << clientAddr << std::endl;
    } else {
        // 连接断开
        uint64_t connectionId = 0;
        
        // 从连接上下文中获取ID
        try {
            connectionId = boost::any_cast<uint64_t>(conn->getContext());
        } catch (const boost::bad_any_cast&) {
            std::cerr << "Failed to get connection ID from context." << std::endl;
            return;
        }
        
        // 调用用户回调
        if (connectionCallback_) {
            connectionCallback_(connectionId, clientAddr, false);
        }
        
        // 注销连接
        UnregisterConnection(connectionId);
        
        std::cout << "Connection [" << connectionId << "] from " 
                  << clientAddr << " closed." << std::endl;
    }
}

void TcpServer::HandleMessage(const TcpConnectionPtr& conn, 
                              muduo::net::Buffer* buffer,
                              muduo::Timestamp timestamp) {
    // 从连接上下文中获取ID
    uint64_t connectionId = 0;
    try {
        connectionId = boost::any_cast<uint64_t>(conn->getContext());
    } catch (const boost::bad_any_cast&) {
        std::cerr << "Failed to get connection ID from context." << std::endl;
        return;
    }
    
    // 获取消息内容
    std::string message = buffer->retrieveAllAsString();
    
    // 调用用户回调
    if (messageCallback_) {
        messageCallback_(connectionId, message, timestamp);
    }
}

uint64_t TcpServer::GenerateConnectionId() {
    static std::mutex mutex;
    std::lock_guard<std::mutex> lock(mutex);
    return nextConnectionId_++;
}

void TcpServer::RegisterConnection(uint64_t id, const TcpConnectionPtr& conn) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_[id] = conn;
}

void TcpServer::UnregisterConnection(uint64_t id) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_.erase(id);
}

TcpServer::TcpConnectionPtr TcpServer::GetConnection(uint64_t id) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    auto it = connections_.find(id);
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr;
}

} // namespace network
} // namespace xumj 