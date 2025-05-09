FROM golang:1.19-alpine AS builder

# 设置工作目录
WORKDIR /app

# 复制go.mod和go.sum文件
COPY go.mod go.sum* ./

# 下载依赖
RUN go mod download

# 复制源代码
COPY . .

# 编译
RUN CGO_ENABLED=0 GOOS=linux go build -a -installsuffix cgo -o api-gateway .

# 最终镜像
FROM alpine:latest

# 安装CA证书
RUN apk --no-cache add ca-certificates

# 工作目录
WORKDIR /root/

# 复制二进制文件
COPY --from=builder /app/api-gateway .

# 暴露端口
EXPOSE 8080

# 运行
CMD ["./api-gateway"] 