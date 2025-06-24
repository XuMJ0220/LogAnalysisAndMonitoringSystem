#ifndef XUMJ_COMMON_LOCK_FREE_QUEUE_H
#define XUMJ_COMMON_LOCK_FREE_QUEUE_H

#include <atomic>
#include <memory>
#include <optional>

namespace xumj {
namespace common {

/*
 * @class LockFreeQueue
 * @brief 高性能无锁队列，用于多线程间的数据交换
 * 
 * 此队列实现了Multiple Producer Single Consumer (MPSC)模式，
 * 允许多个生产者线程并发地向队列添加元素，同时单个消费者线程从队列中取出元素。
 * 通过使用原子操作，避免了传统锁带来的性能开销。
 * 
 * @tparam T 队列中存储的元素类型
 */
template<typename T>
class LockFreeQueue {
private:
    // 队列节点结构
    struct Node {
        std::shared_ptr<T> data;  // 数据指针
        std::atomic<Node*> next;  // 下一个节点的指针
        
        Node() : next(nullptr) {}
        explicit Node(T&& value) : data(std::make_shared<T>(std::move(value))), next(nullptr) {}
        explicit Node(const T& value) : data(std::make_shared<T>(value)), next(nullptr) {}
    };
    
    // 头指针（仅由消费者线程修改）
    std::atomic<Node*> head_;
    
    // 尾指针（可能被多个生产者线程修改，需要使用原子操作）
    std::atomic<Node*> tail_;
    
    // 哑节点，用于简化实现
    Node* dummy_;
    
    // 队列大小计数器
    std::atomic<size_t> size_{0};

public:
    /*
     * @brief 构造函数，初始化队列
     */
    LockFreeQueue() {
        // 创建哑节点
        dummy_ = new Node();
        
        // 初始化头尾指针指向哑节点
        head_.store(dummy_, std::memory_order_relaxed);
        tail_.store(dummy_, std::memory_order_relaxed);
        
        // 初始化大小为0
        size_.store(0, std::memory_order_relaxed);
    }
    
    /*
     * @brief 析构函数，清理队列
     */
    ~LockFreeQueue() {
        // 释放所有节点
        while (Node* node = head_.load(std::memory_order_relaxed)) {
            head_.store(node->next, std::memory_order_relaxed);
            delete node;
        }
    }
    
    /*
     * @brief 向队列中添加元素（移动语义）
     * @param value 要添加的元素
     */
    void Push(T&& value) {
        // 创建新节点
        Node* newNode = new Node(std::move(value));
        
        // 使用原子操作更新尾指针
        Node* oldTail = PushImpl(newNode);
        
        // 释放旧尾节点的下一个指针
        oldTail->next.store(newNode, std::memory_order_release);
        
        // 增加队列大小计数
        size_.fetch_add(1, std::memory_order_relaxed);
    }
    
    /*
     * @brief 向队列中添加元素（拷贝语义）
     * @param value 要添加的元素
     */
    void Push(const T& value) {
        // 创建新节点
        Node* newNode = new Node(value);
        
        // 使用原子操作更新尾指针
        Node* oldTail = PushImpl(newNode);
        
        // 释放旧尾节点的下一个指针
        oldTail->next.store(newNode, std::memory_order_release);
        
        // 增加队列大小计数
        size_.fetch_add(1, std::memory_order_relaxed);
    }
    
    /*
     * @brief 从队列中弹出一个元素
     * @return 如果队列非空，返回队头元素的智能指针；否则返回空
     */
    std::shared_ptr<T> Pop() {
        // 获取当前头节点
        Node* currHead = head_.load(std::memory_order_relaxed);
        Node* nextNode = currHead->next.load(std::memory_order_acquire);
        
        // 如果下一个节点为空，说明队列为空
        if (!nextNode) {
            return nullptr;
        }
        
        // 获取数据
        std::shared_ptr<T> result = nextNode->data;
        
        // 更新头节点
        head_.store(nextNode, std::memory_order_relaxed);
        
        // 删除旧头节点
        delete currHead;
        
        // 减少队列大小计数
        size_.fetch_sub(1, std::memory_order_relaxed);
        
        return result;
    }
    
    /*
     * @brief 检查队列是否为空
     * @return 如果队列为空返回true，否则返回false
     */
    bool IsEmpty() const {
        Node* currHead = head_.load(std::memory_order_relaxed);
        Node* nextNode = currHead->next.load(std::memory_order_acquire);
        return nextNode == nullptr;
    }
    
    /*
     * @brief 获取队列当前大小
     * @return 队列中元素的数量
     */
    size_t Size() const {
        return size_.load(std::memory_order_relaxed);
    }
    
private:
    /*
     * @brief 内部Push实现，处理尾指针的原子更新
     * @param newNode 要添加的新节点
     * @return 旧的尾节点
     */
    Node* PushImpl(Node* newNode) {
        // 循环直到成功更新尾指针
        while (true) {
            // 获取当前尾节点
            Node* currTail = tail_.load(std::memory_order_relaxed);
            
            // 尝试原子地更新尾指针
            if (tail_.compare_exchange_strong(currTail, newNode, 
                                             std::memory_order_acq_rel)) {
                // 如果成功，返回旧的尾节点
                return currTail;
            }
            
            // 如果失败，则重试
        }
    }
    
    // 禁用拷贝构造函数和赋值操作符
    LockFreeQueue(const LockFreeQueue&) = delete;
    LockFreeQueue& operator=(const LockFreeQueue&) = delete;
};

} // namespace common
} // namespace xumj
#endif // XUMJ_COMMON_LOCK_FREE_QUEUE_H 