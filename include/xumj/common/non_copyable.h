#ifndef XUMJ_COMMON_NON_COPYABLE_H
#define XUMJ_COMMON_NON_COPYABLE_H

namespace xumj {
namespace common {

/**
 * @class NonCopyable
 * @brief 禁止拷贝的基类
 * 
 * 继承自此类的派生类将禁止拷贝构造和拷贝赋值操作
 */
class NonCopyable {
protected:
    // 允许构造和析构
    NonCopyable() = default;
    ~NonCopyable() = default;
    
    // 禁止拷贝构造
    NonCopyable(const NonCopyable&) = delete;
    
    // 禁止赋值操作
    NonCopyable& operator=(const NonCopyable&) = delete;
    
    // 允许移动构造和赋值
    NonCopyable(NonCopyable&&) = default;
    NonCopyable& operator=(NonCopyable&&) = default;
};

} // namespace common
} // namespace xumj

#endif // XUMJ_COMMON_NON_COPYABLE_H 