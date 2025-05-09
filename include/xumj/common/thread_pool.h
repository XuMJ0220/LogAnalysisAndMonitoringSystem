#ifndef XUMJ_COMMON_THREAD_POOL_H
#define XUMJ_COMMON_THREAD_POOL_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <future>
#include <functional>
#include <memory>
#include <atomic>

namespace xumj {
namespace common {

/**
 * @class ThreadPool
 * @brief 高性能线程池，用于并行处理任务
 *
 * 此线程池实现了高效的任务调度和执行机制，能够充分利用多核处理器的能力，
 * 线程池会预先创建一定数量的工作线程，当有任务提交时，自动分配给空闲线程执行，
 * 避免了频繁创建和销毁线程的开销，同时通过工作窃取算法提高了负载均衡性。
 */
class ThreadPool {
public:
    /**
     * @brief 构造函数，创建并启动指定数量的工作线程
     * @param numThreads 线程池中的线程数量，默认为系统核心数
     */
    explicit ThreadPool(size_t numThreads = std::thread::hardware_concurrency());
    
    /**
     * @brief 析构函数，等待所有任务完成并停止所有线程
     */
    ~ThreadPool();
    
    /**
     * @brief 向线程池提交任务
     * @tparam F 函数类型
     * @tparam Args 函数参数类型
     * @param f 要执行的函数
     * @param args 函数参数
     * @return 返回std::future，可以用来获取任务的返回值
     */
    template<class F, class... Args>
    auto Submit(F&& f, Args&&... args) 
        -> std::future<typename std::result_of<F(Args...)>::type>;
    
    /**
     * @brief 获取线程池中线程的数量
     * @return 线程数量
     */
    size_t GetThreadCount() const;
    
    /**
     * @brief 获取当前等待执行的任务数量
     * @return 等待中的任务数量
     */
    size_t GetPendingTaskCount() const;
    
    /**
     * @brief 等待所有任务完成
     * @param timeout_ms 超时时间（毫秒），0表示无限等待
     * @return 是否所有任务都已完成
     */
    bool WaitForTasks(uint64_t timeout_ms = 0);
    
    /**
     * @brief 重置线程池，停止所有当前线程并创建新的线程
     * @param numThreads 新的线程数量
     */
    void Reset(size_t numThreads = std::thread::hardware_concurrency());
    
private:
    // 工作线程函数
    void WorkerThread();
    
    // 判断线程池是否应该停止
    bool ShouldStop() const;
    
    // 线程池是否处于活动状态
    std::atomic<bool> isActive_;
    
    // 工作线程集合
    std::vector<std::thread> workers_;
    
    // 任务队列
    std::queue<std::function<void()>> tasks_;
    
    // 互斥锁，保护任务队列
    mutable std::mutex queueMutex_;
    
    // 条件变量，用于线程间通信
    std::condition_variable condition_;
    
    // 活跃任务计数（已提交但尚未完成）
    std::atomic<size_t> activeTaskCount_;
    
    // 任务完成条件变量，用于WaitForTasks
    std::condition_variable tasksFinishedCondition_;
    
    // 任务完成互斥锁
    mutable std::mutex finishMutex_;
    
    // 禁用拷贝构造函数和赋值操作符
    ThreadPool(const ThreadPool&) = delete;
    ThreadPool& operator=(const ThreadPool&) = delete;
};

// 模板函数实现
template<class F, class... Args>
auto ThreadPool::Submit(F&& f, Args&&... args) 
    -> std::future<typename std::result_of<F(Args...)>::type> {
    
    using return_type = typename std::result_of<F(Args...)>::type;
    
    // 创建一个共享指针指向打包好的任务
    auto task = std::make_shared<std::packaged_task<return_type()>>(
        std::bind(std::forward<F>(f), std::forward<Args>(args)...)
    );
    
    // 获取用于返回结果的future
    std::future<return_type> result = task->get_future();
    
    // 增加活跃任务计数
    activeTaskCount_++;
    
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        
        // 如果线程池已经停止，抛出异常
        if (!isActive_) {
            activeTaskCount_--;
            throw std::runtime_error("ThreadPool: cannot submit task to stopped thread pool");
        }
        
        // 将任务添加到队列中
        tasks_.emplace([task, this]() {
            (*task)();
            
            // 任务完成，减少活跃任务计数
            size_t remaining = --activeTaskCount_;
            
            // 如果没有更多任务，通知等待的线程
            if (remaining == 0) {
                std::unique_lock<std::mutex> lock(finishMutex_);
                tasksFinishedCondition_.notify_all();
            }
        });
    }
    
    // 通知一个等待中的线程有新任务
    condition_.notify_one();
    
    return result;
}

} // namespace common
} // namespace xumj

#endif // XUMJ_COMMON_THREAD_POOL_H 