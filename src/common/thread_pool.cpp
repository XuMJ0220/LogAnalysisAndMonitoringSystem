#include "xumj/common/thread_pool.h"
#include <chrono>

namespace xumj {
namespace common {

ThreadPool::ThreadPool(size_t numThreads)
    : isActive_(true), activeTaskCount_(0) {
    // 确保至少有一个线程
    numThreads = numThreads > 0 ? numThreads : 1;
    
    // 创建工作线程
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back(&ThreadPool::WorkerThread, this);
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        isActive_ = false;  // 标记线程池为非活动状态
    }
    
    // 通知所有线程，唤醒它们以便检查isActive_标志
    condition_.notify_all();
    
    // 等待所有线程完成
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

size_t ThreadPool::GetThreadCount() const {
    return workers_.size();
}

size_t ThreadPool::GetPendingTaskCount() const {
    std::unique_lock<std::mutex> lock(queueMutex_);
    return tasks_.size();
}

bool ThreadPool::WaitForTasks(uint64_t timeout_ms) {
    // 如果没有活跃的任务，直接返回true
    if (activeTaskCount_ == 0) {
        return true;
    }
    
    std::unique_lock<std::mutex> lock(finishMutex_);
    
    // 如果指定了超时时间
    if (timeout_ms > 0) {
        // 等待条件变量，直到超时或者所有任务完成
        return tasksFinishedCondition_.wait_for(lock, 
            std::chrono::milliseconds(timeout_ms),
            [this] { return activeTaskCount_ == 0; });
    } else {
        // 无限等待，直到所有任务完成
        tasksFinishedCondition_.wait(lock,
            [this] { return activeTaskCount_ == 0; });
        return true;
    }
}

void ThreadPool::Reset(size_t numThreads) {
    // 首先停止当前的所有线程
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        isActive_ = false;
    }
    
    // 通知所有线程
    condition_.notify_all();
    
    // 等待所有线程结束
    for (auto& worker : workers_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    
    // 清空线程容器
    workers_.clear();
    
    // 清空任务队列
    {
        std::unique_lock<std::mutex> lock(queueMutex_);
        while (!tasks_.empty()) {
            tasks_.pop();
        }
        isActive_ = true;  // 重新激活线程池
    }
    
    // 重置活跃任务计数
    activeTaskCount_ = 0;
    
    // 创建新的线程
    numThreads = numThreads > 0 ? numThreads : 1;
    for (size_t i = 0; i < numThreads; ++i) {
        workers_.emplace_back(&ThreadPool::WorkerThread, this);
    }
}

void ThreadPool::WorkerThread() {
    // 线程循环，不断检查和执行任务
    while (!ShouldStop()) {
        std::function<void()> task;
        
        {
            // 获取任务队列的锁
            std::unique_lock<std::mutex> lock(queueMutex_);
            
            // 等待条件变量，直到有任务或者线程池停止
            condition_.wait(lock, [this] {
                return !isActive_ || !tasks_.empty();
            });
            
            // 如果线程池已停止且没有任务，则退出
            if (!isActive_ && tasks_.empty()) {
                return;
            }
            
            // 获取一个任务并从队列中移除
            task = std::move(tasks_.front());
            tasks_.pop();
        }
        
        // 执行任务
        try {
            task();
        } catch (...) {
            // 捕获所有异常，防止线程崩溃
            // 在实际应用中，可能需要日志记录或其他处理
        }
    }
}

bool ThreadPool::ShouldStop() const {
    std::unique_lock<std::mutex> lock(queueMutex_);
    return !isActive_ && tasks_.empty();
}

} // namespace common
} // namespace xumj 