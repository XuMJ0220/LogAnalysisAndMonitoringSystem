#include <xumj/network/tcp_server.h>
#include <iostream>
#include <sstream>
#include <mutex>
#include <thread>
#include <chrono>
#include <boost/any.hpp>

using namespace muduo;
using namespace muduo::net;

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
      nextConnectionId_(1),
      loop_(nullptr),
      server_(nullptr) {
    
    // std::cout << "创建TcpServer - " << serverName_ << " 在 " 
    //           << listenAddr_ << ":" << port_ << std::endl;
    
    // 注意：不在构造函数中创建EventLoop，而是在Start()方法中创建
    // 这样可以确保EventLoop在正确的线程中创建和使用
    
    std::cout << "INFO: TCP Server [" << serverName_ << "] created at " 
              << listenAddr_ << ":" << port_ << " with " 
              << (numThreads_ == 0 ? std::thread::hardware_concurrency() : numThreads_) << " threads." 
              << std::endl;
}

TcpServer::~TcpServer() {
    Stop();
}

void TcpServer::Start() {
    if (running_) {
        std::cout << "WARNING: TcpServer [" << serverName_ << "] 已经在运行中" << std::endl;
        return;
    }
    
    std::cout << "INFO: 开始启动TcpServer - " << serverName_ << std::endl;
    
    // 创建并启动事件循环线程
    loopThread_ = std::make_unique<std::thread>([this]() {
        // 在事件循环线程中创建EventLoop
        try {
            std::cout << "INFO: 事件循环线程启动..." << std::endl;
            
            // 创建EventLoop（在当前线程中）
            EventLoop loop;
            {
                std::lock_guard<std::mutex> lock(shutdownMutex_);
                loop_ = &loop;
            }
            
            // 创建服务器地址
            InetAddress serverAddr(listenAddr_, port_);
            
            // 创建TCP服务器 - 启用地址复用选项
            muduo::net::TcpServer server(&loop, serverAddr, serverName_, muduo::net::TcpServer::Option::kReusePort);
            {
                std::lock_guard<std::mutex> lock(shutdownMutex_);
                server_ = &server;
            }
            
            // 设置线程数量
            if (numThreads_ == 0) {
                numThreads_ = std::thread::hardware_concurrency();
            }
            server.setThreadNum(numThreads_);
            
            // 使用Lambda表达式设置回调
            server.setConnectionCallback(
                [this](const muduo::net::TcpConnectionPtr& conn) {
                    this->HandleConnection(conn, conn->connected());
                }
            );
            
            server.setMessageCallback(
                [this](const muduo::net::TcpConnectionPtr& conn, 
                       muduo::net::Buffer* buffer,
                       muduo::Timestamp timestamp) {
                    this->HandleMessage(conn, buffer, timestamp);
                }
            );
            
            // 这里将server保存为成员变量，以便在其他线程中访问
            {
                std::lock_guard<std::mutex> lock(shutdownMutex_);
                running_ = true;
            }
            
            // 启动服务器
            server.start();
            std::cout << "TCP Server [" << serverName_ << "] started." << std::endl;
            
            // 通知等待线程服务器已启动
            shutdownCv_.notify_all();
            
            // 运行事件循环，这会阻塞当前线程直到loop.quit()被调用
            loop.loop();
            
            // 事件循环结束后，清除指针
            {
                std::lock_guard<std::mutex> lock(shutdownMutex_);
                server_ = nullptr;
                loop_ = nullptr;
                running_ = false;
            }
            
            std::cout << "事件循环线程结束。" << std::endl;
        }
        catch (const std::exception& e) {
            std::cerr << "异常: 事件循环线程执行失败: " << e.what() << std::endl;
            
            // 设置服务器为非运行状态
            std::lock_guard<std::mutex> lock(shutdownMutex_);
            running_ = false;
            server_ = nullptr;
            loop_ = nullptr;
            shutdownCv_.notify_all();
        }
    });
    
    // 等待服务器启动完成或超时
    {
        std::unique_lock<std::mutex> lock(shutdownMutex_);
        if (!running_) {
            shutdownCv_.wait_for(lock, std::chrono::seconds(5), [this]() { return running_; });
            
            if (!running_) {
                std::cerr << "错误: 服务器启动超时!" << std::endl;
                
                // 如果线程还活着，尝试结束它
                if (loopThread_ && loopThread_->joinable()) {
                    loopThread_->detach();  // 分离线程，让它自己结束
                    loopThread_.reset();
                }
                
                return;
            }
        }
    }
}

void TcpServer::Stop() {
    // 使用临时变量保存状态，避免多次调用
    bool wasRunning = false;
    
    {
        std::lock_guard<std::mutex> lock(shutdownMutex_);
        wasRunning = running_;
        
        if (wasRunning && loop_) {
            // 在事件循环线程中停止服务器
            loop_->queueInLoop([this]() {
                std::cout << "DEBUG: 停止TcpServer - " << serverName_ << std::endl;
                
                // 清理所有连接 (在事件循环线程中安全进行)
                {
                    std::lock_guard<std::mutex> connLock(connectionsMutex_);
                    connections_.clear();
                }
                
                // 退出事件循环
                loop_->quit();
            });
        }
    }
    
    // 等待事件循环线程结束
    if (wasRunning && loopThread_ && loopThread_->joinable()) {
        loopThread_->join();
        loopThread_.reset();
        
        std::cout << "TCP Server [" << serverName_ << "] stopped." << std::endl;
    }
}

bool TcpServer::Send(uint64_t connectionId, const std::string& message) {
    TcpConnectionPtr conn = GetConnection(connectionId);
    
    if (conn && conn->connected()) {
        // 获取连接所属的事件循环
        EventLoop* connLoop = conn->getLoop();
        
        // 在连接所属的线程中执行发送
        connLoop->runInLoop([conn, message]() {
            conn->send(message);
        });
        return true;
    }
    
    return false;
}

size_t TcpServer::Broadcast(const std::string& message) {
    size_t count = 0;
    std::vector<TcpConnectionPtr> activeConns;
    
    // 首先收集所有活跃的连接
    {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        for (const auto& pair : connections_) {
            if (pair.second && pair.second->connected()) {
                activeConns.push_back(pair.second);
                count++;
            }
        }
    }
    
    // 对每个连接执行发送操作
    for (const auto& conn : activeConns) {
        // 在连接所属的线程中执行发送
        EventLoop* connLoop = conn->getLoop();
        connLoop->runInLoop([conn, message]() {
            conn->send(message);
        });
    }
    
    return count;
}

bool TcpServer::CloseConnection(uint64_t connectionId) {
    auto conn = GetConnection(connectionId);
    if (conn) {
        // 在连接所属的线程中执行关闭操作
        EventLoop* connLoop = conn->getLoop();
        connLoop->runInLoop([conn]() {
            conn->shutdown();
        });
        return true;
    }
    return false;
}

size_t TcpServer::GetConnectionCount() const {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    return connections_.size();
}

muduo::net::TcpConnectionPtr TcpServer::GetConnection(uint64_t id) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    auto it = connections_.find(id);
    if (it != connections_.end()) {
        return it->second;
    }
    return nullptr;
}

void TcpServer::HandleConnection(const muduo::net::TcpConnectionPtr& conn, bool connected) {
    // 获取客户端地址
    std::string clientAddr = conn->peerAddress().toIpPort();
    std::string localAddr = conn->localAddress().toIpPort();
    
    std::cout << "===== TCP连接事件 =====" << std::endl;
    std::cout << "- 本地地址: " << localAddr << std::endl;
    std::cout << "- 远程地址: " << clientAddr << std::endl;
    std::cout << "- 连接状态: " << (connected ? "已建立" : "已断开") << std::endl;
    
    if (connected) {
        // 新连接建立
        uint64_t connectionId = nextConnectionId_++;
        
        std::cout << "- 分配连接ID: " << connectionId << std::endl;
        
        // 在连接上存储ID
        conn->setContext(connectionId);
        
        // 注册连接
        RegisterConnection(connectionId, conn);
        
        // 调用用户回调
        if (connectionCallback_) {
            std::cout << "===== 新TCP连接 =====" << std::endl;
            std::cout << "- 服务器: " << serverName_ << std::endl;
            std::cout << "- 连接ID: " << connectionId << std::endl;
            std::cout << "- 客户端: " << clientAddr << std::endl;
            std::cout << "- 当前连接数: " << GetConnectionCount() << std::endl;
            std::cout << "===================" << std::endl;
            
            try {
                connectionCallback_(connectionId, clientAddr, true);
            } catch (const std::exception& e) {
                std::cerr << "错误: 执行连接回调时异常: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "错误: 执行连接回调时未知异常" << std::endl;
            }
        } else {
            std::cerr << "警告: 未设置连接回调函数，无法通知新连接" << std::endl;
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
            std::cout << "===== TCP连接断开 =====" << std::endl;
            std::cout << "- 服务器: " << serverName_ << std::endl;
            std::cout << "- 连接ID: " << connectionId << std::endl;
            std::cout << "- 客户端: " << clientAddr << std::endl;
            std::cout << "======================" << std::endl;
            
            try {
                connectionCallback_(connectionId, clientAddr, false);
            } catch (const std::exception& e) {
                std::cerr << "错误: 执行连接回调时异常: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "错误: 执行连接回调时未知异常" << std::endl;
            }
        } else {
            std::cerr << "警告: 未设置连接回调函数，无法通知连接断开" << std::endl;
        }
        
        // 注销连接
        UnregisterConnection(connectionId);
        
        std::cout << "Connection [" << connectionId << "] from " 
                  << clientAddr << " closed." << std::endl;
    }
}

void TcpServer::HandleMessage(const muduo::net::TcpConnectionPtr& conn, 
                             muduo::net::Buffer* buffer,
                             muduo::Timestamp timestamp) {
    // 打印调试信息
    std::cout << "收到数据，大小：" << buffer->readableBytes() << " 字节" << std::endl;
    
    // 调试：打印当前buffer内容的十六进制表示
    // const char* data = buffer->peek();
    // size_t len = buffer->readableBytes();
    // std::cout << "原始数据十六进制: ";
    // for (size_t i = 0; i < std::min(len, size_t(100)); ++i) {
    //     printf("%02X ", static_cast<unsigned char>(data[i]));
    // }
    // if (len > 100) {
    //     std::cout << "... (总共 " << len << " 字节)";
    // }
    // std::cout << std::endl;
    
    // // 打印可读字符表示
    // std::string peekData = len <= 1024 ? 
    //     std::string(data, len) : 
    //     std::string(data, 1024) + "... (总共 " + std::to_string(len) + " 字节)";
    // std::cout << "原始数据: [" << peekData << "]" << std::endl;
    
    // 查找对应的连接ID
    uint64_t connectionId = 0;
    try {
        connectionId = boost::any_cast<uint64_t>(conn->getContext());
    } catch (const boost::bad_any_cast&) {
        std::cerr << "Failed to get connection ID from context." << std::endl;
        connectionId = 0;
    }
    
    // 如果找不到connectionId，通过连接对象查找
    if (connectionId == 0) {
        std::lock_guard<std::mutex> lock(connectionsMutex_);
        for (const auto& kv : connections_) {
            if (kv.second.get() == conn.get()) {
                connectionId = kv.first;
                break;
            }
        }
    }
    
    // 简化处理逻辑：尝试获取整个消息
    if (buffer->readableBytes() > 0) {
        std::string allData(buffer->peek(), buffer->readableBytes());
        std::cout << "收到完整消息: [" << (allData.size() <= 1024 ? allData : allData.substr(0, 1024) + "...") 
                 << "]" << std::endl;
        
        // 移除所有数据
        buffer->retrieveAll();
        
        // 如果有回调，调用它
        if (connectionId > 0 && messageCallback_) {
            try {
                messageCallback_(connectionId, allData, timestamp);
            } catch (const std::exception& e) {
                std::cerr << "错误: 执行消息回调时异常: " << e.what() << std::endl;
            } catch (...) {
                std::cerr << "错误: 执行消息回调时未知异常" << std::endl;
            }
        } else {
            std::cerr << "TcpServer - 消息回调失败: " 
                    << (connectionId <= 0 ? "找不到连接ID" : "未设置回调函数")
                    << std::endl;
        }
    }
}

void TcpServer::RegisterConnection(uint64_t id, const muduo::net::TcpConnectionPtr& conn) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_[id] = conn;
}

void TcpServer::UnregisterConnection(uint64_t id) {
    std::lock_guard<std::mutex> lock(connectionsMutex_);
    connections_.erase(id);
}

} // namespace network
} // namespace xumj 