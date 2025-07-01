#include <log4cpp/Category.hh>
#include <log4cpp/FileAppender.hh>
#include <log4cpp/PatternLayout.hh>
#include <thread>
#include <chrono>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <fstream>
#include "faker/PhoneNumber.h"
#include "faker/Company.h"
#include "faker/Name.h"
#include "faker/json.hpp"
#include <sys/stat.h>
#include <sys/types.h>
using json = nlohmann::json;

std::string random_ip() {
    char buf[16];
    snprintf(buf, sizeof(buf), "%d.%d.%d.%d", rand()%256, rand()%256, rand()%256, rand()%256);
    return buf;
}

std::string random_sentence() {
    static const char* sentences[] = {"操作成功", "参数错误", "权限不足", "系统异常", "网络超时"};
    return sentences[rand() % 5];
}

std::string random_action() {
    static const char* actions[] = {"登录", "下单", "支付", "查询", "登出"};
    return actions[rand() % 5];
}

std::string get_time_str() {
    char buf[32];
    std::time_t t = std::time(nullptr);
    std::strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", std::localtime(&t));
    return buf;
}

int main() {
    // 读取配置
    std::ifstream config_file("../config.json");
    json config;
    config_file >> config;

    int logs_per_second = config.value("logs_per_second", 10);
    int total_logs = config.value("total_logs", 1000);
    int duration_seconds = config.value("duration_seconds", 0);
    std::string output_file = "logs/test_service.log";
    if (config.contains("output_file")) {
        output_file = config["output_file"];
    }
    // 自动创建日志目录
    size_t last_slash = output_file.find_last_of("/");
    if (last_slash != std::string::npos) {
        std::string dir = output_file.substr(0, last_slash);
        struct stat st = {0};
        if (stat(dir.c_str(), &st) == -1) {
            mkdir(dir.c_str(), 0755);
        }
    }
    auto level_dist = config.value("level_distribution", json{{"info",70},{"warn",20},{"error",10}});
    std::vector<std::string> modules = config.value("modules", std::vector<std::string>{"user","order","payment","system"});
    std::string locale = config.value("locale", "en");
    std::string log_format = config.value("log_format", "{time} [{level}] {module}: {msg}");
    bool console_output = config.value("console_output", false);
    std::vector<std::string> fields = config.value("fields", std::vector<std::string>{"name","phone","company","ip","action","sentence"});

    Faker::Base::setConfigDir("locales");
    srand(time(nullptr));
    log4cpp::PatternLayout* layout = new log4cpp::PatternLayout();
    layout->setConversionPattern("%d [%p] %c: %m%n");
    log4cpp::Appender* appender = new log4cpp::FileAppender("default", output_file);
    appender->setLayout(layout);
    log4cpp::Category& root = log4cpp::Category::getRoot();
    root.setAppender(appender);
    root.setPriority(log4cpp::Priority::DEBUG);

    // 级别分布
    std::vector<std::string> levels;
    for (auto& kv : level_dist.items()) {
        for (int i = 0; i < kv.value(); ++i) levels.push_back(kv.key());
    }

    int log_count = 0;
    auto start = std::chrono::steady_clock::now();
    while ((total_logs == 0 || log_count < total_logs) && (duration_seconds == 0 || std::chrono::duration_cast<std::chrono::seconds>(std::chrono::steady_clock::now()-start).count() < duration_seconds)) {
        for (int i = 0; i < logs_per_second; ++i) {
            std::string name = std::find(fields.begin(), fields.end(), "name")!=fields.end() ? Faker::Name::name() : "";
            std::string phone = std::find(fields.begin(), fields.end(), "phone")!=fields.end() ? Faker::PhoneNumber::phone_number() : "";
            std::string company = std::find(fields.begin(), fields.end(), "company")!=fields.end() ? Faker::Company::name() : "";
            std::string action = std::find(fields.begin(), fields.end(), "action")!=fields.end() ? random_action() : "";
            std::string sentence = std::find(fields.begin(), fields.end(), "sentence")!=fields.end() ? random_sentence() : "";
            std::string ip = std::find(fields.begin(), fields.end(), "ip")!=fields.end() ? random_ip() : "";
            std::string module = modules[rand() % modules.size()];
            std::string level = levels[rand() % levels.size()];
            std::string msg = "";
            if (log_format.find("{msg}") != std::string::npos) {
                msg = "用户:" + name + " 手机:" + phone + " 公司:" + company + " IP:" + ip + " 模块:" + module + " 操作:" + action + " 内容:" + sentence;
            }
            std::string logline = log_format;
            // 替换变量
            size_t pos;
            while ((pos = logline.find("{time}")) != std::string::npos) logline.replace(pos, 6, get_time_str());
            while ((pos = logline.find("{level}")) != std::string::npos) logline.replace(pos, 7, level);
            while ((pos = logline.find("{module}")) != std::string::npos) logline.replace(pos, 8, module);
            while ((pos = logline.find("{msg}")) != std::string::npos) logline.replace(pos, 5, msg);
            if (level == "info") root.info(logline);
            else if (level == "warn") root.warn(logline);
            else root.error(logline);
            if (console_output) std::cout << logline << std::endl;
            ++log_count;
            if (total_logs && log_count >= total_logs) break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    log4cpp::Category::shutdown();
    return 0;
} 