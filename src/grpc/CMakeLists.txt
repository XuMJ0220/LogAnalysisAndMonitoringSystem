# src/grpc/CMakeLists.txt

# 查找gRPC和Protobuf依赖
find_package(Protobuf REQUIRED)
find_package(gRPC REQUIRED)

# 使用预先生成的Proto文件
set(PROTO_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/proto_gen")

# 设置Proto源文件列表
set(PROTO_SRCS
    "${PROTO_GEN_DIR}/alert_service.pb.cc"
    "${PROTO_GEN_DIR}/log_service.pb.cc"
)

# 设置Proto头文件列表
set(PROTO_HDRS
    "${PROTO_GEN_DIR}/alert_service.pb.h"
    "${PROTO_GEN_DIR}/log_service.pb.h"
)

# 设置gRPC源文件列表
set(GRPC_SRCS
    "${PROTO_GEN_DIR}/alert_service.grpc.pb.cc"
    "${PROTO_GEN_DIR}/log_service.grpc.pb.cc"
)

# 设置gRPC头文件列表
set(GRPC_HDRS
    "${PROTO_GEN_DIR}/alert_service.grpc.pb.h"
    "${PROTO_GEN_DIR}/log_service.grpc.pb.h"
)

# 添加gRPC库
add_library(grpc_server STATIC
    log_service_impl.cpp
    alert_service_impl.cpp
    ${PROTO_SRCS}
    ${GRPC_SRCS}
)

# 添加对proto_gen目标的依赖
add_dependencies(grpc_server proto_gen)

# 设置C++17标准
set_target_properties(grpc_server PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 添加编译选项
target_compile_options(grpc_server PRIVATE -Wall -Wextra)

# 添加包含目录
target_include_directories(grpc_server PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
    ${PROTO_GEN_DIR}
    ${PROTOBUF_INCLUDE_DIRS}
    ${GRPC_INCLUDE_DIRS}
)

# 链接依赖库
target_link_libraries(grpc_server
    processor
    analyzer
    storage
    collector
    alert
    common
    ${PROTOBUF_LIBRARIES}
    ${GRPC_LIBRARIES}
) 