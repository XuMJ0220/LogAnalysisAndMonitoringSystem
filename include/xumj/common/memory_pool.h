#ifndef XUMJ_COMMON_MEMORY_POOL_H
#define XUMJ_COMMON_MEMORY_POOL_H

#include <vector>
#include <mutex>
#include <memory>
#include <cassert>
#include <cstddef>
#include <unordered_map>

namespace xumj {
namespace common {

/**
 * @class MemoryPool
 * @brief 高性能内存池，减少内存分配和回收的开销
 * 
 * 此内存池实现了对象池模式，预先分配一定数量的内存块，
 * 当需要分配内存时，从池中获取现成的内存块，避免系统调用；
 * 当释放内存时，不真正释放，而是将其标记为可重用状态，
 * 以便后续分配请求重用，从而减少内存碎片和提高性能。
 */
class MemoryPool {
public:
    /**
     * @brief 构造函数
     * @param chunkSize 每个内存块的大小（字节）
     * @param initialSize 初始预分配的内存块数量
     */
    explicit MemoryPool(size_t chunkSize, size_t initialSize = 1024);
    
    /**
     * @brief 析构函数，释放所有预分配的内存
     */
    ~MemoryPool();
    
    /**
     * @brief 分配指定大小的内存
     * @param size 需要分配的内存大小（字节）
     * @return 分配的内存指针，若分配失败则返回nullptr
     */
    void* Allocate(size_t size);
    
    /**
     * @brief 释放之前分配的内存
     * @param ptr 待释放的内存指针
     * @return 操作是否成功
     */
    bool Deallocate(void* ptr);
    
    /**
     * @brief 获取内存池中当前已分配的内存块数量
     * @return 已分配的内存块数量
     */
    size_t GetAllocatedCount() const;
    
    /**
     * @brief 获取内存池中当前空闲的内存块数量
     * @return 空闲的内存块数量
     */
    size_t GetFreeCount() const;
    
    /**
     * @brief 重置内存池，释放所有已分配的内存
     */
    void Reset();
    
    /**
     * @brief 自适应调整内存池大小
     * @param targetFreeCount 目标空闲内存块数量
     */
    void Shrink(size_t targetFreeCount = 128);
    
private:
    // 内存块的大小
    const size_t chunkSize_;
    
    // 管理空闲内存块的列表
    std::vector<void*> freeChunks_;
    
    // 记录所有分配的内存块，用于析构时统一释放
    std::vector<void*> allocatedChunks_;
    
    // 记录每个内存指针对应的块大小
    std::unordered_map<void*, size_t> ptrToSizeMap_;
    
    // 互斥锁，保护内存池的并发访问
    mutable std::mutex mutex_;
    
    /**
     * @brief 分配一组新的内存块
     * @param numChunks 要分配的内存块数量
     */
    void AllocateChunks(size_t numChunks);
    
    // 禁用拷贝构造函数和赋值操作符
    MemoryPool(const MemoryPool&) = delete;
    MemoryPool& operator=(const MemoryPool&) = delete;
};

/**
 * @class ObjectPool
 * @brief 基于内存池的对象池，用于高效地分配和回收特定类型的对象
 * @tparam T 对象类型
 */
template <typename T>
class ObjectPool {
public:
    /**
     * @brief 构造函数
     * @param initialSize 初始预分配的对象数量
     */
    explicit ObjectPool(size_t initialSize = 1024)
        : memoryPool_(sizeof(T), initialSize) {}
    
    /**
     * @brief 创建一个新对象
     * @tparam Args 构造函数参数类型
     * @param args 传递给对象构造函数的参数
     * @return 指向新创建对象的智能指针
     */
    template <typename... Args>
    std::shared_ptr<T> Acquire(Args&&... args) {
        void* memory = memoryPool_.Allocate(sizeof(T));
        if (!memory) {
            return nullptr;
        }
        
        // 在分配的内存上构造对象
        T* object = new (memory) T(std::forward<Args>(args)...);
        
        // 创建自定义删除器的共享指针，确保对象被正确析构并返回内存池
        return std::shared_ptr<T>(object, [this](T* obj) {
            // 调用对象的析构函数
            obj->~T();
            
            // 将内存返回给内存池
            memoryPool_.Deallocate(static_cast<void*>(obj));
        });
    }
    
    /**
     * @brief 获取对象池中已分配对象的数量
     * @return 已分配的对象数量
     */
    size_t GetAllocatedCount() const {
        return memoryPool_.GetAllocatedCount();
    }
    
    /**
     * @brief 获取对象池中空闲对象的数量
     * @return 空闲的对象数量
     */
    size_t GetFreeCount() const {
        return memoryPool_.GetFreeCount();
    }
    
    /**
     * @brief 重置对象池
     */
    void Reset() {
        memoryPool_.Reset();
    }
    
    /**
     * @brief 收缩对象池
     * @param targetFreeCount 目标空闲对象数量
     */
    void Shrink(size_t targetFreeCount = 128) {
        memoryPool_.Shrink(targetFreeCount);
    }
    
private:
    MemoryPool memoryPool_;
};

} // namespace common
} // namespace xumj

#endif // XUMJ_COMMON_MEMORY_POOL_H 