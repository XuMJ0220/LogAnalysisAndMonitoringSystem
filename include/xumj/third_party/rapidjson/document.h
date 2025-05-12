#pragma once

#include <string>
#include <stdexcept>
#include <iostream>

namespace rapidjson {

enum ParseErrorCode {
    kParseErrorNone = 0,
    kParseErrorDocumentEmpty,
    kParseErrorDocumentRootNotSingular,
    kParseErrorValueInvalid,
    kParseErrorObjectMissName,
    kParseErrorObjectMissColon,
    kParseErrorObjectMissCommaOrCurlyBracket,
    kParseErrorArrayMissCommaOrSquareBracket,
    kParseErrorStringUnicodeEscapeInvalidHex,
    kParseErrorStringUnicodeSurrogateInvalid,
    kParseErrorStringEscapeInvalid,
    kParseErrorStringMissQuotationMark,
    kParseErrorStringInvalidEncoding,
    kParseErrorNumberTooBig,
    kParseErrorNumberMissFraction,
    kParseErrorNumberMissExponent,
    kParseErrorTermination,
    kParseErrorUnspecificSyntaxError
};

// 简化的Value类，用于测试
class Value {
public:
    Value() : type_(kNullType), stringValue_(""), intValue_(0), doubleValue_(0.0), boolValue_(false) {}
    
    enum Type {
        kNullType = 0,
        kFalseType,
        kTrueType,
        kObjectType,
        kArrayType,
        kStringType,
        kNumberType
    };
    
    // 基本类型检查
    bool IsNull() const { return type_ == kNullType; }
    bool IsBool() const { return type_ == kTrueType || type_ == kFalseType; }
    bool IsObject() const { return type_ == kObjectType; }
    bool IsArray() const { return type_ == kArrayType; }
    bool IsString() const { return type_ == kStringType; }
    bool IsNumber() const { return type_ == kNumberType; }
    
    // 获取值
    const char* GetString() const { 
        if (!IsString()) throw std::runtime_error("Not a string");
        return stringValue_.c_str();
    }
    
    int GetInt() const {
        if (!IsNumber()) throw std::runtime_error("Not a number");
        return intValue_;
    }
    
    double GetDouble() const {
        if (!IsNumber()) throw std::runtime_error("Not a number");
        return doubleValue_;
    }
    
    bool GetBool() const {
        if (!IsBool()) throw std::runtime_error("Not a boolean");
        return boolValue_;
    }
    
    // 设置值（仅用于测试）
    void SetString(const std::string& value) {
        type_ = kStringType;
        stringValue_ = value;
    }
    
    void SetInt(int value) {
        type_ = kNumberType;
        intValue_ = value;
        doubleValue_ = static_cast<double>(value);
    }
    
    void SetDouble(double value) {
        type_ = kNumberType;
        doubleValue_ = value;
        intValue_ = static_cast<int>(value);
    }
    
    void SetBool(bool value) {
        type_ = value ? kTrueType : kFalseType;
        boolValue_ = value;
    }
    
    void SetNull() {
        type_ = kNullType;
    }
    
protected:  // 改为protected，让子类可以访问
    Type type_;
    std::string stringValue_;
    int intValue_;
    double doubleValue_;
    bool boolValue_;
};

// 简化的Document类，用于测试
class Document : public Value {
public:
    Document() : parseError_(kParseErrorNone) {}
    
    // 解析JSON字符串
    void Parse(const char* json) {
        // 这是一个非常简化的解析器，仅用于测试
        // 实际上，我们会根据传入的JSON字符串设置一些预定义的值
        
        std::string jsonStr(json);
        
        // 检查是否为空JSON
        if (jsonStr.empty() || jsonStr == "{}") {
            parseError_ = kParseErrorDocumentEmpty;
            return;
        }
        
        // 检查是否为非JSON字符串
        if (jsonStr.find('{') == std::string::npos || jsonStr.find('}') == std::string::npos) {
            parseError_ = kParseErrorUnspecificSyntaxError;
            return;
        }
        
        // 设置为对象类型
        type_ = kObjectType;  // 现在可以访问了
        
        // 解析一些常见字段（非常简化的实现）
        if (jsonStr.find("\"id\"") != std::string::npos) {
            hasId_ = true;
        }
        
        if (jsonStr.find("\"log_id\"") != std::string::npos) {
            hasLogId_ = true;
        }
        
        if (jsonStr.find("\"timestamp\"") != std::string::npos) {
            hasTimestamp_ = true;
        }
        
        if (jsonStr.find("\"time\"") != std::string::npos) {
            hasTime_ = true;
        }
        
        if (jsonStr.find("\"@timestamp\"") != std::string::npos) {
            hasAtTimestamp_ = true;
        }
        
        if (jsonStr.find("\"level\"") != std::string::npos) {
            hasLevel_ = true;
        }
        
        if (jsonStr.find("\"severity\"") != std::string::npos) {
            hasSeverity_ = true;
        }
        
        if (jsonStr.find("\"source\"") != std::string::npos) {
            hasSource_ = true;
        }
        
        if (jsonStr.find("\"service\"") != std::string::npos) {
            hasService_ = true;
        }
        
        if (jsonStr.find("\"message\"") != std::string::npos) {
            hasMessage_ = true;
        }
        
        if (jsonStr.find("\"msg\"") != std::string::npos) {
            hasMsg_ = true;
        }
    }
    
    // 检查是否有解析错误
    bool HasParseError() const {
        return parseError_ != kParseErrorNone;
    }
    
    // 获取解析错误代码
    ParseErrorCode GetParseError() const {
        return parseError_;
    }
    
    // 检查是否有指定成员
    bool HasMember(const char* name) const {
        std::string memberName(name);
        
        if (memberName == "id") return hasId_;
        if (memberName == "log_id") return hasLogId_;
        if (memberName == "timestamp") return hasTimestamp_;
        if (memberName == "time") return hasTime_;
        if (memberName == "@timestamp") return hasAtTimestamp_;
        if (memberName == "level") return hasLevel_;
        if (memberName == "severity") return hasSeverity_;
        if (memberName == "source") return hasSource_;
        if (memberName == "service") return hasService_;
        if (memberName == "message") return hasMessage_;
        if (memberName == "msg") return hasMsg_;
        
        return false;
    }
    
    // 获取成员值
    const Value& operator[](const char* name) const {
        std::string memberName(name);
        
        // 为了简化测试，我们为每个支持的字段返回一个预定义的值
        static Value nullValue;
        static Value idValue;
        static Value timestampValue;
        static Value levelValue;
        static Value sourceValue;
        static Value messageValue;
        
        if (memberName == "id" && hasId_) {
            idValue.SetString("test-id-123");
            return idValue;
        }
        
        if (memberName == "log_id" && hasLogId_) {
            idValue.SetString("test-log-id-456");
            return idValue;
        }
        
        if (memberName == "timestamp" && hasTimestamp_) {
            timestampValue.SetString("2023-05-10T12:34:56Z");
            return timestampValue;
        }
        
        if (memberName == "time" && hasTime_) {
            timestampValue.SetString("2023-05-10T12:34:56Z");
            return timestampValue;
        }
        
        if (memberName == "@timestamp" && hasAtTimestamp_) {
            timestampValue.SetString("2023-05-10T12:34:56Z");
            return timestampValue;
        }
        
        if (memberName == "level" && hasLevel_) {
            levelValue.SetString("INFO");
            return levelValue;
        }
        
        if (memberName == "severity" && hasSeverity_) {
            levelValue.SetString("ERROR");
            return levelValue;
        }
        
        if (memberName == "source" && hasSource_) {
            sourceValue.SetString("test-service");
            return sourceValue;
        }
        
        if (memberName == "service" && hasService_) {
            sourceValue.SetString("another-service");
            return sourceValue;
        }
        
        if (memberName == "message" && hasMessage_) {
            messageValue.SetString("This is a test message");
            return messageValue;
        }
        
        if (memberName == "msg" && hasMsg_) {
            messageValue.SetString("This is another test message");
            return messageValue;
        }
        
        // 如果字段不存在，返回空值
        nullValue.SetNull();
        return nullValue;
    }
    
    // 用于Writer
    template <typename Writer>
    bool Accept(Writer& writer) const {
        // 简化实现，仅用于测试
        writer.StartObject();
        
        if (hasId_) {
            writer.String("id");
            writer.String("test-id-123");
        }
        
        if (hasTimestamp_) {
            writer.String("timestamp");
            writer.String("2023-05-10T12:34:56Z");
        }
        
        if (hasLevel_) {
            writer.String("level");
            writer.String("INFO");
        }
        
        if (hasSource_) {
            writer.String("source");
            writer.String("test-service");
        }
        
        if (hasMessage_) {
            writer.String("message");
            writer.String("This is a test message");
        }
        
        writer.EndObject();
        return true;
    }
    
private:
    ParseErrorCode parseError_;
    bool hasId_ = false;
    bool hasLogId_ = false;
    bool hasTimestamp_ = false;
    bool hasTime_ = false;
    bool hasAtTimestamp_ = false;
    bool hasLevel_ = false;
    bool hasSeverity_ = false;
    bool hasSource_ = false;
    bool hasService_ = false;
    bool hasMessage_ = false;
    bool hasMsg_ = false;
};

} // namespace rapidjson 