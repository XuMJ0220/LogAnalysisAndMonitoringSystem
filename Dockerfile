FROM ubuntu:22.04 as builder

# 安装构建工具和依赖项
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libprotobuf-dev \
    protobuf-compiler \
    libgrpc++-dev \
    protobuf-compiler-grpc \
    libgtest-dev \
    libhiredis-dev \
    libmysqlclient-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    libboost-all-dev \
    pkg-config

# 创建工作目录
WORKDIR /app

# 复制CMake文件
COPY CMakeLists.txt .
COPY src/CMakeLists.txt src/
COPY src/collector/CMakeLists.txt src/collector/
COPY src/processor/CMakeLists.txt src/processor/
COPY src/analyzer/CMakeLists.txt src/analyzer/
COPY src/storage/CMakeLists.txt src/storage/
COPY src/alert/CMakeLists.txt src/alert/
COPY src/common/CMakeLists.txt src/common/
COPY src/network/CMakeLists.txt src/network/
COPY src/grpc/CMakeLists.txt src/grpc/
COPY examples/CMakeLists.txt examples/
COPY tests/CMakeLists.txt tests/

# 复制源代码
COPY src/ src/
COPY examples/ examples/
COPY tests/ tests/
COPY proto/ proto/

# 创建构建目录
RUN mkdir -p build

# 构建
WORKDIR /app/build
RUN cmake .. -DCMAKE_BUILD_TYPE=Release
RUN make -j$(nproc)

# 创建最终镜像
FROM ubuntu:22.04

# 安装运行时依赖
RUN apt-get update && apt-get install -y \
    libprotobuf-dev \
    libgrpc++-dev \
    libhiredis-dev \
    libmysqlclient-dev \
    libssl-dev \
    libcurl4-openssl-dev \
    libboost-all-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# 创建工作目录
WORKDIR /app

# 从构建阶段复制编译好的程序和库
COPY --from=builder /app/build/src/grpc/libgrpc_server.so /usr/local/lib/
COPY --from=builder /app/build/bin/* /app/bin/

# 设置库路径
ENV LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# 创建配置目录
RUN mkdir -p /app/config

# 创建日志目录
RUN mkdir -p /app/logs

# 设置工作目录
WORKDIR /app

# 暴露端口
EXPOSE 50051

# 启动服务
CMD ["/app/bin/log_service"] 