#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include <memory>
#include <thread>
#include <chrono>
#include <grpcpp/grpcpp.h>

#include "../proto/log_service.grpc.pb.h"
#include "../proto/alert_service.grpc.pb.h"
#include "../src/grpc/log_service_impl.h"
#include "../src/grpc/alert_service_impl.h"
#include "../src/collector/log_collector.h"
#include "../src/processor/log_processor.h"
#include "../src/analyzer/log_analyzer.h"
#include "../src/alert/alert_manager.h"
#include "../src/storage/storage_factory.h"

using namespace testing;
using namespace xumj;

class ServiceIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建依赖的组件
        collector_ = std::make_shared<collector::LogCollector>();
        processor_ = std::make_shared<processor::LogProcessor>();
        analyzer_ = std::make_shared<analyzer::LogAnalyzer>();
        alertManager_ = std::make_shared<alert::AlertManager>();
        
        // 配置存储
        auto redisConfig = storage::RedisStorageConfig();
        redisConfig.host = "localhost";
        redisConfig.port = 6379;
        
        auto mysqlConfig = storage::MySQLStorageConfig();
        mysqlConfig.host = "localhost";
        mysqlConfig.port = 3306;
        mysqlConfig.username = "test";
        mysqlConfig.password = "test";
        mysqlConfig.database = "logtest";
        
        auto storageFactory = std::make_shared<storage::StorageFactory>();
        redisStorage_ = storageFactory->CreateRedisStorage(redisConfig);
        mysqlStorage_ = storageFactory->CreateMySQLStorage(mysqlConfig);
        
        // 设置组件间的关联
        processor_->SetLogCollector(collector_);
        analyzer_->SetLogProcessor(processor_);
        analyzer_->SetAlertManager(alertManager_);
        
        // 创建gRPC服务实现
        logService_ = std::make_shared<grpc::LogServiceImpl>(
            collector_, processor_, analyzer_, redisStorage_, mysqlStorage_
        );
        
        alertService_ = std::make_shared<grpc::AlertServiceImpl>(alertManager_);
        
        // 创建gRPC服务器
        std::string serverAddress("localhost:50051");
        ::grpc::ServerBuilder builder;
        builder.AddListeningPort(serverAddress, ::grpc::InsecureServerCredentials());
        builder.RegisterService(logService_.get());
        builder.RegisterService(alertService_.get());
        server_ = builder.BuildAndStart();
        
        // 创建通道和存根
        channel_ = ::grpc::CreateChannel(serverAddress, ::grpc::InsecureChannelCredentials());
        logStub_ = xumj::proto::LogService::NewStub(channel_);
        alertStub_ = xumj::proto::AlertService::NewStub(channel_);
        
        // 给服务器一些启动时间
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    void TearDown() override {
        server_->Shutdown();
    }
    
    // 辅助方法：提交日志
    bool SubmitTestLogs(int count) {
        ::grpc::ClientContext context;
        xumj::proto::LogBatch batch;
        xumj::proto::SubmitResponse response;
        
        for (int i = 0; i < count; ++i) {
            auto* entry = batch.add_entries();
            entry->set_log_id("test-log-" + std::to_string(i));
            entry->set_timestamp(std::to_string(
                std::chrono::system_clock::to_time_t(std::chrono::system_clock::now())
            ));
            entry->set_source("test-source");
            entry->set_level(xumj::proto::LogLevel::INFO);
            entry->set_content("测试日志内容 " + std::to_string(i));
            
            // 添加一些字段
            (*entry->mutable_fields())["count"] = std::to_string(i);
            (*entry->mutable_fields())["type"] = "test";
            
            // 每5个日志添加一个错误级别的日志
            if (i % 5 == 0) {
                entry->set_level(xumj::proto::LogLevel::ERROR);
                entry->set_content("错误日志内容 " + std::to_string(i));
                (*entry->mutable_fields())["error_code"] = "500";
            }
        }
        
        ::grpc::Status status = logStub_->SubmitLogs(&context, batch, &response);
        return status.ok() && response.success();
    }
    
    // 辅助方法：添加测试告警规则
    std::string AddTestAlertRule() {
        ::grpc::ClientContext context;
        xumj::proto::AlertRule rule;
        xumj::proto::CreateAlertRuleResponse response;
        
        rule.set_name("测试阈值规则");
        rule.set_description("当错误数量超过阈值时触发告警");
        rule.set_type(xumj::proto::AlertRuleType::THRESHOLD);
        rule.set_level(xumj::proto::AlertLevel::ERROR);
        rule.set_enabled(true);
        
        auto* thresholdConfig = rule.mutable_threshold_config();
        thresholdConfig->set_field("error_count");
        thresholdConfig->set_threshold(3);
        thresholdConfig->set_compare_type(">");
        
        ::grpc::Status status = alertStub_->CreateAlertRule(&context, rule, &response);
        if (status.ok() && response.success()) {
            return response.rule_id();
        }
        return "";
    }
    
    // 组件
    std::shared_ptr<collector::LogCollector> collector_;
    std::shared_ptr<processor::LogProcessor> processor_;
    std::shared_ptr<analyzer::LogAnalyzer> analyzer_;
    std::shared_ptr<alert::AlertManager> alertManager_;
    std::shared_ptr<storage::RedisStorage> redisStorage_;
    std::shared_ptr<storage::MySQLStorage> mysqlStorage_;
    
    // gRPC服务
    std::shared_ptr<grpc::LogServiceImpl> logService_;
    std::shared_ptr<grpc::AlertServiceImpl> alertService_;
    
    // gRPC服务器
    std::unique_ptr<::grpc::Server> server_;
    
    // gRPC客户端
    std::shared_ptr<::grpc::Channel> channel_;
    std::unique_ptr<xumj::proto::LogService::Stub> logStub_;
    std::unique_ptr<xumj::proto::AlertService::Stub> alertStub_;
};

// 测试日志提交和查询
TEST_F(ServiceIntegrationTest, LogSubmitAndQuery) {
    // 提交日志
    ASSERT_TRUE(SubmitTestLogs(10));
    
    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 查询日志
    ::grpc::ClientContext context;
    xumj::proto::LogQuery query;
    xumj::proto::LogQueryResponse response;
    
    query.set_limit(100);
    query.set_offset(0);
    
    ::grpc::Status status = logStub_->QueryLogs(&context, query, &response);
    
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(response.success());
    ASSERT_GE(response.logs_size(), 10);
}

// 测试日志统计
TEST_F(ServiceIntegrationTest, LogStats) {
    // 提交日志
    ASSERT_TRUE(SubmitTestLogs(20));
    
    // 等待处理完成
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    
    // 获取统计信息
    ::grpc::ClientContext context;
    xumj::proto::StatsRequest request;
    xumj::proto::StatsResponse response;
    
    ::grpc::Status status = logStub_->GetStats(&context, request, &response);
    
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(response.success());
    ASSERT_GE(response.total_logs(), 20);
    ASSERT_GE(response.error_logs(), 4); // 每5个日志有1个错误日志
}

// 测试告警规则创建和查询
TEST_F(ServiceIntegrationTest, AlertRuleCreateAndQuery) {
    // 添加告警规则
    std::string ruleId = AddTestAlertRule();
    ASSERT_FALSE(ruleId.empty());
    
    // 查询告警规则
    ::grpc::ClientContext context;
    xumj::proto::AlertRuleQuery query;
    xumj::proto::AlertRuleQueryResponse response;
    
    ::grpc::Status status = alertStub_->GetAlertRules(&context, query, &response);
    
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(response.success());
    ASSERT_GE(response.rules_size(), 1);
    
    bool foundRule = false;
    for (int i = 0; i < response.rules_size(); ++i) {
        if (response.rules(i).id() == ruleId) {
            foundRule = true;
            break;
        }
    }
    
    ASSERT_TRUE(foundRule);
}

// 测试日志提交触发告警
TEST_F(ServiceIntegrationTest, LogTriggersAlert) {
    // 添加告警规则
    std::string ruleId = AddTestAlertRule();
    ASSERT_FALSE(ruleId.empty());
    
    // 配置分析器使用该规则
    alertManager_->SetAlertCallback([](const std::string& alertId, alert::AlertStatus status) {
        // 不做任何事情，只是为了触发告警
    });
    
    // 提交足够多的错误日志以触发告警
    ASSERT_TRUE(SubmitTestLogs(50)); // 包含10个错误日志
    
    // 等待处理和分析完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 查询告警
    ::grpc::ClientContext context;
    xumj::proto::AlertQuery query;
    xumj::proto::AlertQueryResponse response;
    
    ::grpc::Status status = alertStub_->GetAlerts(&context, query, &response);
    
    ASSERT_TRUE(status.ok());
    ASSERT_TRUE(response.success());
    ASSERT_GE(response.alerts_size(), 1);
}

// 测试告警状态更新
TEST_F(ServiceIntegrationTest, AlertStatusUpdate) {
    // 添加告警规则
    std::string ruleId = AddTestAlertRule();
    ASSERT_FALSE(ruleId.empty());
    
    // 配置分析器使用该规则
    alertManager_->SetAlertCallback([](const std::string& alertId, alert::AlertStatus status) {
        // 不做任何事情，只是为了触发告警
    });
    
    // 提交足够多的错误日志以触发告警
    ASSERT_TRUE(SubmitTestLogs(50)); // 包含10个错误日志
    
    // 等待处理和分析完成
    std::this_thread::sleep_for(std::chrono::seconds(1));
    
    // 查询告警
    ::grpc::ClientContext queryContext;
    xumj::proto::AlertQuery query;
    xumj::proto::AlertQueryResponse queryResponse;
    
    ::grpc::Status queryStatus = alertStub_->GetAlerts(&queryContext, query, &queryResponse);
    
    ASSERT_TRUE(queryStatus.ok());
    ASSERT_TRUE(queryResponse.success());
    ASSERT_GE(queryResponse.alerts_size(), 1);
    
    // 更新第一个告警的状态
    std::string alertId = queryResponse.alerts(0).id();
    ::grpc::ClientContext updateContext;
    xumj::proto::UpdateAlertStatusRequest updateRequest;
    xumj::proto::UpdateAlertStatusResponse updateResponse;
    
    updateRequest.set_alert_id(alertId);
    updateRequest.set_status(static_cast<int>(xumj::proto::AlertStatus::RESOLVED));
    updateRequest.set_comment("集成测试解决告警");
    
    ::grpc::Status updateStatus = alertStub_->UpdateAlertStatus(&updateContext, updateRequest, &updateResponse);
    
    ASSERT_TRUE(updateStatus.ok());
    ASSERT_TRUE(updateResponse.success());
    ASSERT_EQ(updateResponse.updated_alert().id(), alertId);
    ASSERT_EQ(updateResponse.updated_alert().status(), xumj::proto::AlertStatus::RESOLVED);
} 