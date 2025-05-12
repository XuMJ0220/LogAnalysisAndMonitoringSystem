#pragma once

#include <string>
#include <memory>

namespace xumj {

class LogEntry {
public:
    LogEntry() = default;
    ~LogEntry() = default;

    // Getters
    const std::string& GetId() const { return id_; }
    const std::string& GetTimestamp() const { return timestamp_; }
    const std::string& GetLevel() const { return level_; }
    const std::string& GetSource() const { return source_; }
    const std::string& GetMessage() const { return message_; }

    // Setters
    void SetId(const std::string& id) { id_ = id; }
    void SetTimestamp(const std::string& timestamp) { timestamp_ = timestamp; }
    void SetLevel(const std::string& level) { level_ = level; }
    void SetSource(const std::string& source) { source_ = source; }
    void SetMessage(const std::string& message) { message_ = message; }

private:
    std::string id_;
    std::string timestamp_;
    std::string level_;
    std::string source_;
    std::string message_;
};

} // namespace xumj 