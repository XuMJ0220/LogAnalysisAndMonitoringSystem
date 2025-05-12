#ifndef XUMJ_NETWORK_TCP_SERVER_H
#define XUMJ_NETWORK_TCP_SERVER_H

#include <string>
#include <memory>
#include <functional>
#include <map>
#include <vector>
#include <mutex>
#include <thread>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Timestamp.h>

namespace xumj {
namespace network {

/**
 * @class TcpServer
 * @brief 高性能TCP服务器，基于muduo网络库实现
 * 
 * 此类封装了muduo的TcpServer，提供了更简洁的接口和额外的功能，
 * 如自动负载均衡、连接统计和流量控制等。主要用于接收来自日志收集
 * 客户端的连接和数据。
 */
class TcpServer {
public:
    /**
     * @brief 消息回调函数类型
     * @param connectionId 连接ID
     * @param message 接收到的消息内容
     * @param timestamp 接收时间戳
     */
    using MessageCallback = std::function<void(uint64_t connectionId, 
                                             const std::string& message,
                                             muduo::Timestamp timestamp)>;
    
    /**
     * @brief 连接回调函数类型
     * @param connectionId 连接ID
     * @param clientAddr 客户端地址
     * @param isConnected 是连接建立还是断开
     */
    using ConnectionCallback = std::function<void(uint64_t connectionId, 
                                                const std::string& clientAddr,
                                                bool isConnected)>;
    
    /**
     * @brief 构造函数
     * @param serverName 服务器名称，用于日志标识
     * @param listenAddr 监听地址
     * @param port 监听端口
     * @param numThreads 网络IO线程数，默认为CPU核心数
     */
    TcpServer(const std::string& serverName,
              const std::string& listenAddr,
              uint16_t port,
              size_t numThreads = 0);
    
    /**
     * @brief 析构函数
     */
    ~TcpServer();
    
    /**
     * @brief 启动服务器
     */
    void Start();
    
    /**
     * @brief 停止服务器
     */
    void Stop();
    
    /**
     * @brief 设置消息回调
     * @param callback 消息回调函数
     */
    void SetMessageCallback(const MessageCallback& callback);
    
    /**
     * @brief 设置连接回调
     * @param callback 连接回调函数
     */
    void SetConnectionCallback(const ConnectionCallback& callback);
    
    /**
     * @brief 向指定连接发送数据
     * @param connectionId 连接ID
     * @param message 要发送的消息
     * @return 是否成功发送
     */
    bool Send(uint64_t connectionId, const std::string& message);
    
    /**
     * @brief 向所有连接广播数据
     * @param message 要发送的消息
     * @return 成功发送的连接数量
     */
    size_t Broadcast(const std::string& message);
    
    /**
     * @brief 关闭指定连接
     * @param connectionId 连接ID
     * @return 是否成功关闭
     */
    bool CloseConnection(uint64_t connectionId);
    
    /**
     * @brief 获取当前活跃连接数
     * @return 活跃连接数
     */
    size_t GetConnectionCount() const;
    
    /**
     * @brief 获取服务器是否正在运行
     * @return 是否正在运行
     */
    bool IsRunning() const;
    
    /**
     * @brief 获取服务器名称
     * @return 服务器名称
     */
    const std::string& GetServerName() const;
    
    /**
     * @brief 获取服务器监听地址
     * @return 监听地址
     */
    const std::string& GetListenAddr() const;
    
    /**
     * @brief 获取服务器监听端口
     * @return 监听端口
     */
    uint16_t GetPort() const;
    
    /**
     * @brief 获取连接指针
     * @param id 连接ID
     * @return 连接指针
     */
    muduo::net::TcpConnectionPtr GetConnection(uint64_t id);
    
private:
    // muduo事件循环
    std::unique_ptr<muduo::net::EventLoop> loop_;
    
    // muduo TCP服务器
    std::unique_ptr<muduo::net::TcpServer> server_;
    
    // 服务器配置信息
    std::string serverName_;
    std::string listenAddr_;
    uint16_t port_;
    size_t numThreads_;
    bool running_;
    
    // 用户回调函数
    MessageCallback messageCallback_;
    ConnectionCallback connectionCallback_;
    
    // 连接管理
    using TcpConnectionPtr = std::shared_ptr<muduo::net::TcpConnection>;
    std::map<uint64_t, TcpConnectionPtr> connections_;
    mutable std::mutex connectionsMutex_;
    uint64_t nextConnectionId_;
    
    // muduo回调处理
    void HandleConnection(const TcpConnectionPtr& conn, bool connected);
    void HandleMessage(const TcpConnectionPtr& conn, 
                      muduo::net::Buffer* buffer,
                      muduo::Timestamp timestamp);
    
    // 生成新的连接ID
    uint64_t GenerateConnectionId();
    
    // 连接ID与连接对象的映射管理
    void RegisterConnection(uint64_t id, const TcpConnectionPtr& conn);
    void UnregisterConnection(uint64_t id);
};

} // namespace network
} // namespace xumj

#endif // XUMJ_NETWORK_TCP_SERVER_H 