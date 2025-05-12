#!/bin/bash

# 切换到项目根目录
cd "$(dirname "$0")/.."

# 创建编译目录（如果不存在）
mkdir -p build/tests

# 编译测试程序
echo "编译解析器基准测试程序..."
g++ -std=c++17 tests/performance/parser_benchmark.cpp -I. -o build/tests/parser_benchmark

# 检查编译是否成功
if [ $? -ne 0 ]; then
    echo "编译失败!"
    exit 1
fi

echo "编译成功!"

# 运行测试程序
echo "运行解析器基准测试..."
build/tests/parser_benchmark

echo "测试完成!" 