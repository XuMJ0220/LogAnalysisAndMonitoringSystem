#!/bin/bash

# 设置颜色
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[0;33m'
NC='\033[0m' # No Color

echo -e "${GREEN}===== 网络模块测试脚本 =====${NC}"

# 检查端口是否被占用
if ss -tulpn | grep -q 9876; then
    echo -e "${YELLOW}警告: 端口9876已被占用，尝试终止进程...${NC}"
    killall network_server_example 2>/dev/null
    sleep 1
fi

# 当前目录
CURR_DIR=$(pwd)
# 定义目录和可执行文件路径
BIN_DIR="$CURR_DIR/build/bin"
SERVER_EXE="$BIN_DIR/network_server_example"
CLIENT_EXE="$BIN_DIR/network_client_example"

# 检查可执行文件是否存在
if [ ! -f "$SERVER_EXE" ] || [ ! -f "$CLIENT_EXE" ]; then
    echo -e "${RED}错误: 找不到可执行文件，请确保已经编译了项目${NC}"
    exit 1
fi

# 在后台启动服务器
echo -e "${GREEN}正在启动服务器...${NC}"
$SERVER_EXE > server_output.log 2>&1 &
SERVER_PID=$!

# 检查服务器是否成功启动
echo -e "${YELLOW}等待服务器启动...${NC}"
sleep 3

if ! ps -p $SERVER_PID > /dev/null; then
    echo -e "${RED}错误: 服务器启动失败${NC}"
    cat server_output.log
    exit 1
fi

# 检查端口是否被监听
if ! ss -tulpn | grep -q 9876; then
    echo -e "${RED}错误: 服务器未能监听端口9876${NC}"
    kill $SERVER_PID 2>/dev/null
    cat server_output.log
    exit 1
fi

echo -e "${GREEN}服务器已成功启动，PID: $SERVER_PID${NC}"

# 启动客户端，发送消息后退出
echo -e "${GREEN}正在启动客户端...${NC}"
(
    echo "正在启动客户端自动测试..."
    sleep 2
    echo "send 测试消息1"
    sleep 1
    echo "send 测试消息2"
    sleep 1
    echo "status"
    sleep 1
    echo "quit"
) | $CLIENT_EXE > client_output.log 2>&1

# 检查客户端输出
echo -e "${GREEN}检查客户端输出...${NC}"
if grep -q "已成功连接到服务器" client_output.log && grep -q "消息已发送" client_output.log; then
    echo -e "${GREEN}客户端成功连接并发送消息${NC}"
    SUCCESS=true
else
    echo -e "${RED}客户端连接或发送消息失败${NC}"
    cat client_output.log
    SUCCESS=false
fi

# 检查服务器日志，查看是否收到消息
echo -e "${GREEN}检查服务器输出...${NC}"
if grep -q "收到消息" server_output.log; then
    echo -e "${GREEN}服务器成功接收消息${NC}"
    SUCCESS=true
else
    echo -e "${RED}服务器未收到客户端消息${NC}"
    SUCCESS=false
fi

# 显示部分服务器日志
echo -e "${YELLOW}最后的服务器日志:${NC}"
tail -n 20 server_output.log

# 终止服务器
echo -e "${GREEN}正在终止服务器...${NC}"
kill $SERVER_PID 2>/dev/null
sleep 1

# 显示测试结果
if [ "$SUCCESS" = true ]; then
    echo -e "${GREEN}===== 测试完成: 成功! =====${NC}"
else
    echo -e "${RED}===== 测试完成: 失败! =====${NC}"
fi

exit 0 