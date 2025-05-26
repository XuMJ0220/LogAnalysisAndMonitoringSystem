#ifndef XUMJ_NETWORK_TCP_CLIENT_H
#define XUMJ_NETWORK_TCP_CLIENT_H

#include <string>
#include <memory>
#include <functional>
#include <mutex>
#include <muduo/net/TcpClient.h>
#include <muduo/net/EventLoop.h>
#include <muduo/net/InetAddress.h>
#include <muduo/base/Timestamp.h>
#include <thread>

namespace xumj {
namespace network {

/*
 * @class TcpClient
 * @brief 高性能TCP客户端，基于muduo网络库实现
 * 
 * 此类封装了muduo的TcpClient，提供了更简洁的接口，
 * 用于与TcpServer通信，支持自动重连和消息处理。
 */
class TcpClient {
public:
    /*
     * @brief 消息回调函数类型
     * @param message 接收到的消息内容
     * @param timestamp 接收时间戳
     */
    using MessageCallback = std::function<void(const std::string& message,
                                             muduo::Timestamp timestamp)>;
    
    /*
     * @brief 连接回调函数类型
     * @param isConnected 是连接建立还是断开
     */
    using ConnectionCallback = std::function<void(bool isConnected)>;
    
    /*
     * @brief 构造函数
     * @param clientName 客户端名称，用于日志标识
     * @param serverAddr 服务器地址
     * @param port 服务器端口
     * @param autoReconnect 是否自动重连
     */
    TcpClient(const std::string& clientName,
              const std::string& serverAddr,
              uint16_t port,
              bool autoReconnect = true);
    
    /*
     * @brief 析构函数
     */
    ~TcpClient();
    
    /*
     * @brief 连接到服务器
     * @return 是否成功发起连接（不代表连接已建立）
     */
    bool Connect();
    
    /*
     * @brief 断开连接
     */
    void Disconnect();
    
    /*
     * @brief 设置消息回调
     * @param callback 消息回调函数
     */
    void SetMessageCallback(const MessageCallback& callback);
    
    /*
     * @brief 设置连接回调
     * @param callback 连接回调函数
     */
    void SetConnectionCallback(const ConnectionCallback& callback);
    
    /*
     * @brief 发送消息到服务器
     * @param message 消息内容
     * @return 成功返回true，失败返回false
     */
    bool Send(const std::string& message);
    
    /*
     * @brief 发送消息到服务器，可选择是否立即刷新缓冲区
     * @param message 消息内容
     * @param flushImmediately 是否立即刷新网络缓冲区
     * @return 成功返回true，失败返回false
     */
    bool Send(const std::string& message, bool flushImmediately);
    
    /*
     * @brief 获取客户端是否已连接
     * @return 是否已连接
     */
    bool IsConnected() const;
    
    /*
     * @brief 获取客户端是否正在重连
     * @return 是否正在重连
     */
    bool IsReconnecting() const;
    
    /*
     * @brief 获取客户端名称
     * @return 客户端名称
     */
    const std::string& GetClientName() const;
    
    /*
     * @brief 获取服务器地址
     * @return 服务器地址
     */
    const std::string& GetServerAddr() const;
    
    /*
     * @brief 获取服务器端口
     * @return 服务器端口
     */
    uint16_t GetServerPort() const;
    
private:
    // 配置信息
    std::string clientName_;
    std::string serverAddr_;
    uint16_t serverPort_;
    bool autoReconnect_;
    
    // 连接状态
    mutable std::mutex stateMutex_;
    bool connected_;
    bool reconnecting_;
    
    // muduo事件循环和客户端
    muduo::net::EventLoop* loop_;
    muduo::net::TcpClient* client_;
    
    // 用户回调函数
    MessageCallback messageCallback_;
    ConnectionCallback connectionCallback_;
    
    // muduo连接对象
    muduo::net::TcpConnectionPtr connection_;
    std::mutex connectionMutex_;
    
    // 事件循环线程
    std::thread loopThread_;
    
    // muduo回调处理
    void HandleConnection(const muduo::net::TcpConnectionPtr& conn);
    void HandleMessage(const muduo::net::TcpConnectionPtr& conn, 
                      muduo::net::Buffer* buffer,
                      muduo::Timestamp timestamp);
    
    // 设置连接状态
    void SetConnected(bool connected);
    void SetReconnecting(bool reconnecting);
    
    // 获取当前连接
    muduo::net::TcpConnectionPtr GetConnection();
};

} // namespace network
} // namespace xumj

#endif // XUMJ_NETWORK_TCP_CLIENT_H 