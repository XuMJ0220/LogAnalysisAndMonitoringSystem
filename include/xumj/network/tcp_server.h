#ifndef XUMJ_NETWORK_TCP_SERVER_H
#define XUMJ_NETWORK_TCP_SERVER_H

#include <string>
#include <map>
#include <functional>
#include <memory>
#include <cstdint>
#include <atomic>
#include <mutex>
#include <thread>
#include <condition_variable>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/Timestamp.h>

namespace xumj {
namespace network {

/*
 * @brief TCP服务器类，包装了muduo的TcpServer
 */
class TcpServer {
public:
    // 回调函数类型
    using MessageCallback = std::function<void(uint64_t, const std::string&, muduo::Timestamp)>;
    using ConnectionCallback = std::function<void(uint64_t, const std::string&, bool)>;
    
    /*
     * @brief 构造函数
     * @param serverName 服务器名称
     * @param listenAddr 监听地址
     * @param port 监听端口
     * @param numThreads 工作线程数，0表示使用系统默认值
     */
    TcpServer(const std::string& serverName,
              const std::string& listenAddr,
              uint16_t port,
              size_t numThreads = 0);
    
    /*
     * @brief 析构函数
     */
    ~TcpServer();
    
    /*
     * @brief 启动服务器
     */
    void Start();

    /*
     * @brief 停止服务器
     */
    void Stop();
    
    /*
     * @brief 发送消息给指定的连接
     * @param connectionId 连接ID
     * @param message 消息内容
     * @return 成功返回true，失败返回false
     */
    bool Send(uint64_t connectionId, const std::string& message);
    
    /*
     * @brief 广播消息给所有连接
     * @param message 消息内容
     * @return 成功发送的连接数
     */
    size_t Broadcast(const std::string& message);
    
    /*
     * @brief 关闭指定连接
     * @param connectionId 连接ID
     * @return 成功返回true，失败返回false
     */
    bool CloseConnection(uint64_t connectionId);
    
    /*
     * @brief 获取当前连接数
     * @return 连接数
     */
    size_t GetConnectionCount() const;
    
    /*
     * @brief 获取指定ID的连接
     * @param id 连接ID
     * @return 连接指针，如果不存在则返回nullptr
     */
    muduo::net::TcpConnectionPtr GetConnection(uint64_t id);
    
    /*
     * @brief 设置消息回调函数
     * @param callback 回调函数
     */
    void SetMessageCallback(const MessageCallback& callback) {
        messageCallback_ = callback;
    }
    
    /*
     * @brief 设置连接回调函数
     * @param callback 回调函数
     */
    void SetConnectionCallback(const ConnectionCallback& callback) {
        connectionCallback_ = callback;
    }
    
    /*
     * @brief 获取服务器名称
     * @return 服务器名称
     */
    const std::string& GetServerName() const {
        return serverName_;
    }
    
    /*
     * @brief 获取监听地址
     * @return 监听地址
     */
    const std::string& GetListenAddr() const {
        return listenAddr_;
    }
    
    /*
     * @brief 获取监听端口
     * @return 监听端口
     */
    uint16_t GetPort() const {
        return port_;
    }
    
    /*
     * @brief 获取线程数
     * @return 线程数
     */
    size_t GetNumThreads() const {
        return numThreads_;
    }
    
    /*
     * @brief 获取服务器运行状态
     * @return 运行状态
     */
    bool IsRunning() const {
        bool running = false;
        {
            std::lock_guard<std::mutex> lock(shutdownMutex_);
            running = running_;
        }
        return running;
    }
    
private:
    std::string serverName_;         // 服务器名称
    std::string listenAddr_;         // 监听地址
    uint16_t port_;                  // 监听端口
    size_t numThreads_;              // 工作线程数
    
    bool running_;                   // 服务器运行状态（需要与互斥锁一起使用）
    std::atomic<uint64_t> nextConnectionId_; // 下一个连接ID
    
    // 互斥锁和条件变量，用于安全停止线程
    mutable std::mutex shutdownMutex_;
    std::condition_variable shutdownCv_;
    
    // Muduo相关 - 使用指针而非智能指针，因为它们的生命周期由事件循环线程管理
    muduo::net::EventLoop* loop_;          // 事件循环指针
    muduo::net::TcpServer* server_;        // TCP服务器指针
    std::unique_ptr<std::thread> loopThread_; // 事件循环线程

    // 连接管理
    mutable std::mutex connectionsMutex_;    // 连接映射表的互斥锁
    std::map<uint64_t, muduo::net::TcpConnectionPtr> connections_; // 连接ID到连接对象的映射
    
    // 回调函数
    ConnectionCallback connectionCallback_; // 连接回调
    MessageCallback messageCallback_;      // 消息回调
    
    // muduo回调处理
    void HandleConnection(const muduo::net::TcpConnectionPtr& conn, bool connected);
    void HandleMessage(const muduo::net::TcpConnectionPtr& conn, 
                       muduo::net::Buffer* buffer,
                       muduo::Timestamp timestamp);
    
    // 连接管理
    uint64_t GenerateConnectionId() {
        return nextConnectionId_++;
    }
    void RegisterConnection(uint64_t id, const muduo::net::TcpConnectionPtr& conn);
    void UnregisterConnection(uint64_t id);
};

} // namespace network
} // namespace xumj 

#endif // XUMJ_NETWORK_TCP_SERVER_H