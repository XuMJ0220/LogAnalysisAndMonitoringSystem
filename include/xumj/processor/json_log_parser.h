#pragma once

#include <string>
#include <memory>
#include "../common/log_entry.h"
#include "../third_party/rapidjson/document.h"
#include "../third_party/rapidjson/stringbuffer.h"
#include "../third_party/rapidjson/writer.h"
#include "../third_party/rapidjson/error/en.h"

namespace xumj {

class JsonLogParser {
public:
    JsonLogParser() = default;
    ~JsonLogParser() = default;

    std::shared_ptr<LogEntry> Parse(const std::string& jsonString);
};

} // namespace xumj 