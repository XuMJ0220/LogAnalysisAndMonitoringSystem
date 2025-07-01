package main

import (
	"database/sql"
	"encoding/json"
	"fmt"
	"log"
	"net/http"
	"os"
	"strconv"
	"strings"
	"time"

	_ "github.com/go-sql-driver/mysql"
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
	port := getEnv("PORT", "18080")
	log.Printf("API网关启动在端口 %s\n", port)
	err := http.ListenAndServe(":"+port, router)
	if err != nil {
		log.Fatalf("ListenAndServe error: %v", err)
	}
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
	queryText := query.Get("query")
	levelFilter := query.Get("level")
	sourceFilter := query.Get("source")
	
	if limit <= 0 {
		limit = 100
	}
	
	// 尝试连接MySQL数据库
	mysqlHost := getEnv("MYSQL_HOST", "localhost")
	mysqlPort := getEnv("MYSQL_PORT", "3306")
	mysqlUser := getEnv("MYSQL_USER", "root")
	mysqlPass := getEnv("MYSQL_PASS", "ytfhqqkso1")
	mysqlDB := getEnv("MYSQL_DB", "logdb")
	
	// 构建DSN连接字符串
	dsn := fmt.Sprintf("%s:%s@tcp(%s:%s)/%s", mysqlUser, mysqlPass, mysqlHost, mysqlPort, mysqlDB)
	
	// 尝试连接到MySQL
	db, err := sql.Open("mysql", dsn)
	if err != nil || db.Ping() != nil {
		// 如果无法连接到MySQL，使用模拟数据
		log.Printf("无法连接到MySQL数据库，使用模拟数据: %v", err)
		
		// 创建模拟日志数据
		mockLogs := []map[string]interface{}{
			{
				"log_id":    "system-log-1",
				"timestamp": time.Now().Unix() - 3600,
				"source":    "system",
				"level":     "INFO",
				"content":   "系统启动成功",
			},
			{
				"log_id":    "application-log-1",
				"timestamp": time.Now().Unix() - 1800,
				"source":    "application",
				"level":     "WARNING",
				"content":   "警告：内存使用率超过80%",
			},
			{
				"log_id":    "database-log-1",
				"timestamp": time.Now().Unix() - 900,
				"source":    "database",
				"level":     "ERROR",
				"content":   "错误：数据库连接失败",
			},
			{
				"log_id":    "api-log-1",
				"timestamp": time.Now().Unix() - 600,
				"source":    "api-gateway",
				"level":     "INFO",
				"content":   "API请求：/api/users，状态：200 OK",
			},
			{
				"log_id":    "security-log-1",
				"timestamp": time.Now().Unix() - 300,
				"source":    "security",
				"level":     "WARNING",
				"content":   "警告：检测到多次失败的登录尝试",
			},
			{
				"log_id":    "system-log-2",
				"timestamp": time.Now().Unix() - 3500,
				"source":    "system",
				"level":     "INFO",
				"content":   "服务器负载：CPU 45%，内存 60%",
			},
			{
				"log_id":    "application-log-2",
				"timestamp": time.Now().Unix() - 1700,
				"source":    "application",
				"level":     "ERROR",
				"content":   "错误：用户认证服务不可用",
			},
			{
				"log_id":    "database-log-2",
				"timestamp": time.Now().Unix() - 800,
				"source":    "database",
				"level":     "INFO",
				"content":   "数据库备份完成，耗时：120秒",
			},
			{
				"log_id":    "api-log-2",
				"timestamp": time.Now().Unix() - 500,
				"source":    "api-gateway",
				"level":     "ERROR",
				"content":   "API请求：/api/orders，状态：500 Internal Server Error",
			},
			{
				"log_id":    "security-log-2",
				"timestamp": time.Now().Unix() - 200,
				"source":    "security",
				"level":     "CRITICAL",
				"content":   "严重：检测到可能的SQL注入攻击",
			},
			{
				"log_id":    "network-log-1",
				"timestamp": time.Now().Unix() - 3200,
				"source":    "network",
				"level":     "WARNING",
				"content":   "网络延迟增加，平均响应时间：300ms",
			},
			{
				"log_id":    "user-log-1",
				"timestamp": time.Now().Unix() - 1500,
				"source":    "user-service",
				"level":     "INFO",
				"content":   "新用户注册：user123@example.com",
			},
			{
				"log_id":    "payment-log-1",
				"timestamp": time.Now().Unix() - 700,
				"source":    "payment-service",
				"level":     "ERROR",
				"content":   "支付处理失败：交易ID TX98765，原因：超时",
			},
			{
				"log_id":    "cache-log-1",
				"timestamp": time.Now().Unix() - 400,
				"source":    "cache-service",
				"level":     "INFO",
				"content":   "缓存清理完成，释放空间：256MB",
			},
			{
				"log_id":    "scheduler-log-1",
				"timestamp": time.Now().Unix() - 100,
				"source":    "task-scheduler",
				"level":     "WARNING",
				"content":   "任务执行延迟：daily-report，延迟时间：15分钟",
			},
		}
		
		// 过滤模拟数据
		filteredLogs := []map[string]interface{}{}
		for _, log := range mockLogs {
			// 检查是否符合搜索条件
			contentMatch := queryText == "" || 
				strings.Contains(log["content"].(string), queryText) || 
				strings.Contains(log["source"].(string), queryText)
			
			// 检查级别过滤
			levelMatch := levelFilter == "" || log["level"].(string) == levelFilter
			
			// 检查来源过滤
			sourceMatch := sourceFilter == "" || log["source"].(string) == sourceFilter
			
			// 如果符合所有条件，添加到结果中
			if contentMatch && levelMatch && sourceMatch {
				filteredLogs = append(filteredLogs, log)
			}
		}
		
		// 应用分页
		startIndex := offset
		endIndex := offset + limit
		if startIndex > len(filteredLogs) {
			startIndex = len(filteredLogs)
		}
		if endIndex > len(filteredLogs) {
			endIndex = len(filteredLogs)
		}
		
		pagedLogs := []map[string]interface{}{}
		if startIndex < endIndex {
			pagedLogs = filteredLogs[startIndex:endIndex]
		}
		
		// 返回结果
		successResponse(w, map[string]interface{}{
			"total_count": len(filteredLogs),
			"has_more":    endIndex < len(filteredLogs),
			"logs":        pagedLogs,
		}, "查询成功（模拟数据）")
		return
	}
	defer db.Close()
	
	// 如果成功连接到MySQL，继续原有的查询逻辑
	// 构建查询SQL
	sqlQuery := "SELECT id, timestamp, source, level, content FROM logs"
	countQuery := "SELECT COUNT(*) FROM logs"
	
	// 添加查询条件
	var whereConditions []string
	var queryParams []interface{}
	
	if queryText != "" {
		whereConditions = append(whereConditions, "(content LIKE ? OR source LIKE ?)")
		queryParams = append(queryParams, "%"+queryText+"%", "%"+queryText+"%")
	}
	
	// 添加级别过滤
	if levelFilter != "" {
		var levelValue int
		switch levelFilter {
		case "TRACE":
			levelValue = 0
		case "DEBUG":
			levelValue = 1
		case "INFO":
			levelValue = 2
		case "WARNING":
			levelValue = 3
		case "ERROR":
			levelValue = 4
		case "CRITICAL":
			levelValue = 5
		}
		whereConditions = append(whereConditions, "level = ?")
		queryParams = append(queryParams, levelValue)
	}
	
	// 添加来源过滤
	if sourceFilter != "" {
		whereConditions = append(whereConditions, "source = ?")
		queryParams = append(queryParams, sourceFilter)
	}
	
	if len(whereConditions) > 0 {
		sqlQuery += " WHERE " + strings.Join(whereConditions, " AND ")
		countQuery += " WHERE " + strings.Join(whereConditions, " AND ")
	}
	
	// 添加排序和分页
	sqlQuery += " ORDER BY timestamp DESC"
	
	// 查询总数
	var totalCount int
	countParams := make([]interface{}, len(queryParams))
	copy(countParams, queryParams)
	err = db.QueryRow(countQuery, countParams...).Scan(&totalCount)
	if err != nil {
		errorResponse(w, "查询日志总数失败: "+err.Error(), http.StatusInternalServerError)
		return
	}
	
	// 添加LIMIT和OFFSET
	sqlQuery += " LIMIT ? OFFSET ?"
	queryParams = append(queryParams, limit, offset)
	
	log.Printf("执行SQL: %s, 参数: %v", sqlQuery, queryParams)
	
	// 执行查询
	rows, err := db.Query(sqlQuery, queryParams...)
	if err != nil {
		errorResponse(w, "查询日志失败: "+err.Error(), http.StatusInternalServerError)
		return
	}
	defer rows.Close()
	
	// 处理结果
	logs := []map[string]interface{}{}
	for rows.Next() {
		var id string
		var timestamp int64
		var source string
		var level int
		var content string
		
		if err := rows.Scan(&id, &timestamp, &source, &level, &content); err != nil {
			errorResponse(w, "解析日志数据失败: "+err.Error(), http.StatusInternalServerError)
			return
		}
		
		// 转换日志级别为字符串
		levelStr := "INFO"
		switch level {
		case 0:
			levelStr = "TRACE"
		case 1:
			levelStr = "DEBUG"
		case 2:
			levelStr = "INFO"
		case 3:
			levelStr = "WARNING"
		case 4:
			levelStr = "ERROR"
		case 5:
			levelStr = "CRITICAL"
		}
		
		logs = append(logs, map[string]interface{}{
			"log_id":    id,
			"timestamp": timestamp,
			"source":    source,
			"level":     levelStr,
			"content":   content,
		})
	}
	
	// 检查是否有更多数据
	hasMore := offset+len(logs) < totalCount
	
	// 记录返回的数据
	log.Printf("返回日志: %d 条, 总数: %d, 偏移量: %d", len(logs), totalCount, offset)
	
	successResponse(w, map[string]interface{}{
		"total_count": totalCount,
		"has_more":    hasMore,
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
	log.Printf("删除规则ID: %s", ruleID) // 使用ruleID变量以避免未使用警告
	successResponse(w, nil, "规则已删除")
} 