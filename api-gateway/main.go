package main

import (
	"context"
	"encoding/json"
	"log"
	"net/http"
	"os"
	"strconv"
	"time"

	"github.com/gorilla/mux"
	"google.golang.org/grpc"
	"google.golang.org/grpc/credentials/insecure"
)

// 服务地址
var (
	logServiceHost   = getEnv("LOG_SERVICE_HOST", "localhost")
	logServicePort   = getEnv("LOG_SERVICE_PORT", "50051")
	alertServiceHost = getEnv("ALERT_SERVICE_HOST", "localhost")
	alertServicePort = getEnv("ALERT_SERVICE_PORT", "50051")
)

// 获取环境变量
func getEnv(key, defaultValue string) string {
	if value, exists := os.LookupEnv(key); exists {
		return value
	}
	return defaultValue
}

// 响应结构
type Response struct {
	Success bool        `json:"success"`
	Message string      `json:"message"`
	Data    interface{} `json:"data,omitempty"`
}

// 错误响应
func errorResponse(w http.ResponseWriter, message string, statusCode int) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(statusCode)
	json.NewEncoder(w).Encode(Response{
		Success: false,
		Message: message,
	})
}

// 成功响应
func successResponse(w http.ResponseWriter, data interface{}, message string) {
	w.Header().Set("Content-Type", "application/json")
	w.WriteHeader(http.StatusOK)
	json.NewEncoder(w).Encode(Response{
		Success: true,
		Message: message,
		Data:    data,
	})
}

// CORS中间件
func corsMiddleware(next http.Handler) http.Handler {
	return http.HandlerFunc(func(w http.ResponseWriter, r *http.Request) {
		w.Header().Set("Access-Control-Allow-Origin", "*")
		w.Header().Set("Access-Control-Allow-Methods", "GET, POST, PUT, DELETE, OPTIONS")
		w.Header().Set("Access-Control-Allow-Headers", "Content-Type, Authorization")
		
		if r.Method == "OPTIONS" {
			w.WriteHeader(http.StatusOK)
			return
		}
		
		next.ServeHTTP(w, r)
	})
}

// 主函数
func main() {
	router := mux.NewRouter()
	
	// 添加中间件
	router.Use(corsMiddleware)
	
	// 健康检查
	router.HandleFunc("/health", func(w http.ResponseWriter, r *http.Request) {
		successResponse(w, nil, "服务正常")
	}).Methods("GET")
	
	// 日志API路由
	logsRouter := router.PathPrefix("/api/logs").Subrouter()
	logsRouter.HandleFunc("", handleSubmitLogs).Methods("POST")
	logsRouter.HandleFunc("", handleQueryLogs).Methods("GET")
	logsRouter.HandleFunc("/stats", handleGetLogStats).Methods("GET")
	
	// 告警API路由
	alertsRouter := router.PathPrefix("/api/alerts").Subrouter()
	alertsRouter.HandleFunc("", handleGetAlerts).Methods("GET")
	alertsRouter.HandleFunc("/{id}", handleGetAlertDetail).Methods("GET")
	alertsRouter.HandleFunc("/{id}/status", handleUpdateAlertStatus).Methods("PUT")
	
	// 告警规则API路由
	rulesRouter := router.PathPrefix("/api/rules").Subrouter()
	rulesRouter.HandleFunc("", handleGetAlertRules).Methods("GET")
	rulesRouter.HandleFunc("", handleCreateAlertRule).Methods("POST")
	rulesRouter.HandleFunc("/{id}", handleUpdateAlertRule).Methods("PUT")
	rulesRouter.HandleFunc("/{id}", handleDeleteAlertRule).Methods("DELETE")
	
	// 启动服务器
	port := getEnv("PORT", "8080")
	log.Printf("API网关启动在端口 %s\n", port)
	log.Fatal(http.ListenAndServe(":"+port, router))
}

// 提交日志处理
func handleSubmitLogs(w http.ResponseWriter, r *http.Request) {
	// 从请求体解析日志批次
	var logBatch map[string]interface{}
	if err := json.NewDecoder(r.Body).Decode(&logBatch); err != nil {
		errorResponse(w, "无效的请求体: "+err.Error(), http.StatusBadRequest)
		return
	}
	
	// 连接日志服务
	conn, err := grpc.Dial(logServiceHost+":"+logServicePort, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		errorResponse(w, "无法连接日志服务: "+err.Error(), http.StatusInternalServerError)
		return
	}
	defer conn.Close()
	
	// 这里应该调用gRPC客户端提交日志
	// client := pb.NewLogServiceClient(conn)
	// resp, err := client.SubmitLogs(context.Background(), &pb.LogBatch{...})
	
	// 由于没有导入生成的protobuf代码，这里只返回模拟响应
	successResponse(w, map[string]interface{}{
		"submitted": len(logBatch),
	}, "日志已提交")
}

// 查询日志处理
func handleQueryLogs(w http.ResponseWriter, r *http.Request) {
	// 解析查询参数
	query := r.URL.Query()
	limit, _ := strconv.Atoi(query.Get("limit"))
	offset, _ := strconv.Atoi(query.Get("offset"))
	
	if limit <= 0 {
		limit = 100
	}
	
	// 连接日志服务
	conn, err := grpc.Dial(logServiceHost+":"+logServicePort, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		errorResponse(w, "无法连接日志服务: "+err.Error(), http.StatusInternalServerError)
		return
	}
	defer conn.Close()
	
	// 这里应该调用gRPC客户端查询日志
	// client := pb.NewLogServiceClient(conn)
	// resp, err := client.QueryLogs(context.Background(), &pb.LogQuery{...})
	
	// 由于没有导入生成的protobuf代码，这里只返回模拟响应
	logs := []map[string]interface{}{
		{
			"log_id":    "sample-log-1",
			"timestamp": time.Now().Unix(),
			"source":    "api-gateway",
			"level":     "INFO",
			"content":   "这是一个示例日志",
		},
	}
	
	successResponse(w, map[string]interface{}{
		"total_count": 1,
		"has_more":    false,
		"logs":        logs,
	}, "查询成功")
}

// 获取日志统计信息
func handleGetLogStats(w http.ResponseWriter, r *http.Request) {
	// 连接日志服务
	conn, err := grpc.Dial(logServiceHost+":"+logServicePort, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		errorResponse(w, "无法连接日志服务: "+err.Error(), http.StatusInternalServerError)
		return
	}
	defer conn.Close()
	
	// 这里应该调用gRPC客户端获取统计信息
	// client := pb.NewLogServiceClient(conn)
	// resp, err := client.GetStats(context.Background(), &pb.StatsRequest{})
	
	// 由于没有导入生成的protobuf代码，这里只返回模拟响应
	stats := map[string]interface{}{
		"total_logs":     1000,
		"error_logs":     50,
		"warning_logs":   200,
		"info_logs":      750,
		"avg_logs_per_day": 100,
	}
	
	successResponse(w, stats, "统计获取成功")
}

// 获取告警列表
func handleGetAlerts(w http.ResponseWriter, r *http.Request) {
	// 解析查询参数
	query := r.URL.Query()
	limit, _ := strconv.Atoi(query.Get("limit"))
	offset, _ := strconv.Atoi(query.Get("offset"))
	
	if limit <= 0 {
		limit = 100
	}
	
	// 连接告警服务
	conn, err := grpc.Dial(alertServiceHost+":"+alertServicePort, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		errorResponse(w, "无法连接告警服务: "+err.Error(), http.StatusInternalServerError)
		return
	}
	defer conn.Close()
	
	// 这里应该调用gRPC客户端获取告警
	// client := pb.NewAlertServiceClient(conn)
	// resp, err := client.GetAlerts(context.Background(), &pb.AlertQuery{...})
	
	// 由于没有导入生成的protobuf代码，这里只返回模拟响应
	alerts := []map[string]interface{}{
		{
			"id":          "sample-alert-1",
			"name":        "高CPU使用率",
			"description": "服务器CPU使用率超过80%",
			"level":       "WARNING",
			"status":      "ACTIVE",
			"timestamp":   time.Now().Unix(),
		},
	}
	
	successResponse(w, map[string]interface{}{
		"total_count": 1,
		"has_more":    false,
		"alerts":      alerts,
	}, "查询成功")
}

// 获取告警详情
func handleGetAlertDetail(w http.ResponseWriter, r *http.Request) {
	// 获取路径参数
	vars := mux.Vars(r)
	alertID := vars["id"]
	
	// 连接告警服务
	conn, err := grpc.Dial(alertServiceHost+":"+alertServicePort, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		errorResponse(w, "无法连接告警服务: "+err.Error(), http.StatusInternalServerError)
		return
	}
	defer conn.Close()
	
	// 这里应该调用gRPC客户端获取告警详情
	// client := pb.NewAlertServiceClient(conn)
	// resp, err := client.GetAlertDetail(context.Background(), &pb.AlertDetailRequest{AlertId: alertID})
	
	// 由于没有导入生成的protobuf代码，这里只返回模拟响应
	alert := map[string]interface{}{
		"id":          alertID,
		"name":        "高CPU使用率",
		"description": "服务器CPU使用率超过80%",
		"level":       "WARNING",
		"status":      "ACTIVE",
		"timestamp":   time.Now().Unix(),
		"source":      "system-monitor",
		"labels": map[string]string{
			"host":    "server-01",
			"service": "web-service",
		},
		"annotations": map[string]string{
			"cpu_value": "85%",
			"threshold": "80%",
		},
		"related_log_ids": []string{"log-1", "log-2"},
		"count":           3,
	}
	
	successResponse(w, alert, "查询成功")
}

// 更新告警状态
func handleUpdateAlertStatus(w http.ResponseWriter, r *http.Request) {
	// 获取路径参数
	vars := mux.Vars(r)
	alertID := vars["id"]
	
	// 从请求体解析状态更新
	var updateRequest map[string]interface{}
	if err := json.NewDecoder(r.Body).Decode(&updateRequest); err != nil {
		errorResponse(w, "无效的请求体: "+err.Error(), http.StatusBadRequest)
		return
	}
	
	// 连接告警服务
	conn, err := grpc.Dial(alertServiceHost+":"+alertServicePort, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		errorResponse(w, "无法连接告警服务: "+err.Error(), http.StatusInternalServerError)
		return
	}
	defer conn.Close()
	
	// 这里应该调用gRPC客户端更新告警状态
	// client := pb.NewAlertServiceClient(conn)
	// resp, err := client.UpdateAlertStatus(context.Background(), &pb.UpdateAlertStatusRequest{...})
	
	// 由于没有导入生成的protobuf代码，这里只返回模拟响应
	updatedAlert := map[string]interface{}{
		"id":          alertID,
		"name":        "高CPU使用率",
		"description": "服务器CPU使用率超过80%",
		"level":       "WARNING",
		"status":      updateRequest["status"],
		"timestamp":   time.Now().Unix(),
	}
	
	successResponse(w, updatedAlert, "状态已更新")
}

// 获取告警规则列表
func handleGetAlertRules(w http.ResponseWriter, r *http.Request) {
	// 解析查询参数
	query := r.URL.Query()
	limit, _ := strconv.Atoi(query.Get("limit"))
	offset, _ := strconv.Atoi(query.Get("offset"))
	
	if limit <= 0 {
		limit = 100
	}
	
	// 连接告警服务
	conn, err := grpc.Dial(alertServiceHost+":"+alertServicePort, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		errorResponse(w, "无法连接告警服务: "+err.Error(), http.StatusInternalServerError)
		return
	}
	defer conn.Close()
	
	// 这里应该调用gRPC客户端获取告警规则
	// client := pb.NewAlertServiceClient(conn)
	// resp, err := client.GetAlertRules(context.Background(), &pb.AlertRuleQuery{...})
	
	// 由于没有导入生成的protobuf代码，这里只返回模拟响应
	rules := []map[string]interface{}{
		{
			"id":          "rule-1",
			"name":        "CPU使用率规则",
			"description": "检测CPU使用率是否超过阈值",
			"level":       "WARNING",
			"type":        "THRESHOLD",
			"enabled":     true,
			"threshold_config": map[string]interface{}{
				"field":       "cpu_usage",
				"threshold":   80,
				"compare_type": ">",
			},
		},
	}
	
	successResponse(w, map[string]interface{}{
		"total_count": 1,
		"has_more":    false,
		"rules":       rules,
	}, "查询成功")
}

// 创建告警规则
func handleCreateAlertRule(w http.ResponseWriter, r *http.Request) {
	// 从请求体解析规则
	var rule map[string]interface{}
	if err := json.NewDecoder(r.Body).Decode(&rule); err != nil {
		errorResponse(w, "无效的请求体: "+err.Error(), http.StatusBadRequest)
		return
	}
	
	// 连接告警服务
	conn, err := grpc.Dial(alertServiceHost+":"+alertServicePort, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		errorResponse(w, "无法连接告警服务: "+err.Error(), http.StatusInternalServerError)
		return
	}
	defer conn.Close()
	
	// 这里应该调用gRPC客户端创建告警规则
	// client := pb.NewAlertServiceClient(conn)
	// resp, err := client.CreateAlertRule(context.Background(), &pb.AlertRule{...})
	
	// 由于没有导入生成的protobuf代码，这里只返回模拟响应
	ruleID := "rule-" + strconv.FormatInt(time.Now().Unix(), 10)
	
	successResponse(w, map[string]interface{}{
		"rule_id": ruleID,
	}, "规则已创建")
}

// 更新告警规则
func handleUpdateAlertRule(w http.ResponseWriter, r *http.Request) {
	// 获取路径参数
	vars := mux.Vars(r)
	ruleID := vars["id"]
	
	// 从请求体解析规则
	var rule map[string]interface{}
	if err := json.NewDecoder(r.Body).Decode(&rule); err != nil {
		errorResponse(w, "无效的请求体: "+err.Error(), http.StatusBadRequest)
		return
	}
	
	// 连接告警服务
	conn, err := grpc.Dial(alertServiceHost+":"+alertServicePort, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		errorResponse(w, "无法连接告警服务: "+err.Error(), http.StatusInternalServerError)
		return
	}
	defer conn.Close()
	
	// 这里应该调用gRPC客户端更新告警规则
	// client := pb.NewAlertServiceClient(conn)
	// resp, err := client.UpdateAlertRule(context.Background(), &pb.AlertRule{...})
	
	// 由于没有导入生成的protobuf代码，这里只返回模拟响应
	successResponse(w, map[string]interface{}{
		"rule_id": ruleID,
	}, "规则已更新")
}

// 删除告警规则
func handleDeleteAlertRule(w http.ResponseWriter, r *http.Request) {
	// 获取路径参数
	vars := mux.Vars(r)
	ruleID := vars["id"]
	
	// 连接告警服务
	conn, err := grpc.Dial(alertServiceHost+":"+alertServicePort, grpc.WithTransportCredentials(insecure.NewCredentials()))
	if err != nil {
		errorResponse(w, "无法连接告警服务: "+err.Error(), http.StatusInternalServerError)
		return
	}
	defer conn.Close()
	
	// 这里应该调用gRPC客户端删除告警规则
	// client := pb.NewAlertServiceClient(conn)
	// resp, err := client.DeleteAlertRule(context.Background(), &pb.DeleteAlertRuleRequest{RuleId: ruleID})
	
	// 由于没有导入生成的protobuf代码，这里只返回模拟响应
	successResponse(w, nil, "规则已删除")
} 