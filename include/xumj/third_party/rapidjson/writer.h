#pragma once

#include <string>
#include "stringbuffer.h"

namespace rapidjson {

// 简化的Writer类，用于测试
template <typename OutputStream>
class Writer {
public:
    Writer(OutputStream& os) : os_(os), inObject_(false), inArray_(false), valueCount_(0) {}
    
    // 开始对象
    bool StartObject() {
        if (inObject_ && valueCount_ > 0) {
            os_.Put(',');
        }
        os_.Put('{');
        inObject_ = true;
        valueCount_ = 0;
        return true;
    }
    
    // 结束对象
    bool EndObject() {
        os_.Put('}');
        if (inObject_) {
            valueCount_++;
        }
        return true;
    }
    
    // 开始数组
    bool StartArray() {
        if (inObject_ && valueCount_ > 0) {
            os_.Put(',');
        }
        os_.Put('[');
        inArray_ = true;
        valueCount_ = 0;
        return true;
    }
    
    // 结束数组
    bool EndArray() {
        os_.Put(']');
        if (inObject_) {
            valueCount_++;
        }
        return true;
    }
    
    // 写入键
    bool Key(const char* str, size_t length) {
        return String(str, length);
    }
    
    bool Key(const char* str) {
        if (inObject_ && valueCount_ > 0) {
            os_.Put(',');
        }
        os_.Put('"');
        os_.PutString(str);
        os_.Put('"');
        os_.Put(':');
        return true;
    }
    
    // 写入字符串
    bool String(const char* str) {
        if (inObject_ && (valueCount_ % 2) == 1) {
            // 值
            os_.Put('"');
            os_.PutString(str);
            os_.Put('"');
            valueCount_++;
        } else if (inObject_) {
            // 键
            if (valueCount_ > 0) {
                os_.Put(',');
            }
            os_.Put('"');
            os_.PutString(str);
            os_.Put('"');
            os_.Put(':');
            valueCount_++;
        } else {
            // 不在对象中
            os_.Put('"');
            os_.PutString(str);
            os_.Put('"');
        }
        return true;
    }
    
    bool String(const char* str, size_t length) {
        return String(str);
    }
    
    // 写入整数
    bool Int(int i) {
        if (inObject_ && valueCount_ > 0 && (valueCount_ % 2) == 0) {
            os_.Put(',');
        }
        os_.PutString(std::to_string(i).c_str());
        if (inObject_) {
            valueCount_++;
        }
        return true;
    }
    
    // 写入双精度浮点数
    bool Double(double d) {
        if (inObject_ && valueCount_ > 0 && (valueCount_ % 2) == 0) {
            os_.Put(',');
        }
        os_.PutString(std::to_string(d).c_str());
        if (inObject_) {
            valueCount_++;
        }
        return true;
    }
    
    // 写入布尔值
    bool Bool(bool b) {
        if (inObject_ && valueCount_ > 0 && (valueCount_ % 2) == 0) {
            os_.Put(',');
        }
        os_.PutString(b ? "true" : "false");
        if (inObject_) {
            valueCount_++;
        }
        return true;
    }
    
    // 写入null
    bool Null() {
        if (inObject_ && valueCount_ > 0 && (valueCount_ % 2) == 0) {
            os_.Put(',');
        }
        os_.PutString("null");
        if (inObject_) {
            valueCount_++;
        }
        return true;
    }
    
private:
    OutputStream& os_;
    bool inObject_;
    bool inArray_;
    int valueCount_;
};

} // namespace rapidjson 