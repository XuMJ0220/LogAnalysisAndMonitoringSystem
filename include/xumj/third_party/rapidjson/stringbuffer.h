#pragma once

#include <string>

namespace rapidjson {

// 简化的StringBuffer类，用于测试
class StringBuffer {
public:
    StringBuffer() : str_() {}
    
    // 添加字符
    void Put(char c) {
        str_ += c;
    }
    
    // 添加字符串
    void PutString(const char* str) {
        str_ += str;
    }
    
    // 获取字符串
    const char* GetString() const {
        return str_.c_str();
    }
    
    // 获取大小
    size_t GetSize() const {
        return str_.size();
    }
    
    // 清空
    void Clear() {
        str_.clear();
    }
    
private:
    std::string str_;
};

} // namespace rapidjson 