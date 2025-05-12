#include "xumj/common/memory_pool.h"
#include <cstdlib>
#include <algorithm>
#include <stdexcept>

namespace xumj {
namespace common {

MemoryPool::MemoryPool(size_t chunkSize, size_t initialSize)
    : chunkSize_(chunkSize == 0 ? 1 : chunkSize) {
    // 预分配初始内存块
    if (initialSize > 0) {
        AllocateChunks(initialSize);
    }
}

MemoryPool::~MemoryPool() {
    // 释放所有分配的内存块
    for (auto chunk : allocatedChunks_) {
        std::free(chunk);
    }
    
    // 清空列表
    freeChunks_.clear();
    allocatedChunks_.clear();
    ptrToSizeMap_.clear();
}

void* MemoryPool::Allocate(size_t size) {
    // 如果请求的大小超过预设的块大小，则直接从系统分配
    if (size > chunkSize_) {
        void* ptr = std::malloc(size);
        if (ptr) {
            std::lock_guard<std::mutex> lock(mutex_);
            // 记录这个指针和它的实际大小，用于后续释放
            ptrToSizeMap_[ptr] = size;
        }
        return ptr;
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果没有可用的空闲块，则分配一批新的
    if (freeChunks_.empty()) {
        // 每次分配的块数量逐步增加，提高扩展效率
        size_t newChunks = std::max<size_t>(64, allocatedChunks_.size() / 2);
        AllocateChunks(newChunks);
        
        // 如果分配失败，则返回nullptr
        if (freeChunks_.empty()) {
            return nullptr;
        }
    }
    
    // 从空闲列表中取出一个块
    void* chunk = freeChunks_.back();
    freeChunks_.pop_back();
    
    // 记录这个指针对应的实际块大小
    ptrToSizeMap_[chunk] = chunkSize_;
    
    return chunk;
}

bool MemoryPool::Deallocate(void* ptr) {
    if (!ptr) {
        return false;  // 空指针不需要释放
    }
    
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 查找这个指针对应的块大小
    auto it = ptrToSizeMap_.find(ptr);
    if (it == ptrToSizeMap_.end()) {
        return false;  // 不是由这个内存池分配的内存，或者是已经被释放的内存
    }
    
    size_t size = it->second;
    ptrToSizeMap_.erase(it);
    
    // 如果是大于标准块大小的内存，直接释放
    if (size > chunkSize_) {
        std::free(ptr);
        return true;
    }
    
    // 将内存块放回空闲列表
    freeChunks_.push_back(ptr);
    return true;
}

size_t MemoryPool::GetAllocatedCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return allocatedChunks_.size();
}

size_t MemoryPool::GetFreeCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return freeChunks_.size();
}

void MemoryPool::Reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 将所有已分配但尚未释放的内存块重置为空闲状态
    freeChunks_.clear();
    
    for (void* chunk : allocatedChunks_) {
        freeChunks_.push_back(chunk);
    }
    
    // 清除所有指针到大小的映射
    ptrToSizeMap_.clear();
}

void MemoryPool::Shrink(size_t targetFreeCount) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 如果空闲块数量少于目标值，不需要收缩
    if (freeChunks_.size() <= targetFreeCount) {
        return;
    }
    
    // 释放超出目标数量的空闲块
    size_t excessCount = freeChunks_.size() - targetFreeCount;
    for (size_t i = 0; i < excessCount; ++i) {
        void* chunk = freeChunks_.back();
        freeChunks_.pop_back();
        
        // 从已分配列表中找到并移除这个块
        auto it = std::find(allocatedChunks_.begin(), allocatedChunks_.end(), chunk);
        if (it != allocatedChunks_.end()) {
            std::swap(*it, allocatedChunks_.back());
            allocatedChunks_.pop_back();
        }
        
        // 释放内存
        std::free(chunk);
    }
}

void MemoryPool::AllocateChunks(size_t numChunks) {
    for (size_t i = 0; i < numChunks; ++i) {
        // 分配一个块
        void* chunk = std::malloc(chunkSize_);
        if (!chunk) {
            // 内存分配失败，停止继续分配
            break;
        }
        
        // 将新分配的块添加到列表中
        freeChunks_.push_back(chunk);//这里不能用emplace_back，因为下面还需要用到chunk
        allocatedChunks_.push_back(chunk);//这里可以用emplace_back
    }
}

} // namespace common
} // namespace xumj 