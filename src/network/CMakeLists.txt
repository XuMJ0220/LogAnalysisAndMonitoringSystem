# src/network/CMakeLists.txt

# 暂时添加一个空的网络库，后续会逐步实现
add_library(network STATIC
    tcp_server.cpp
    tcp_client.cpp
)

# 设置编译选项
target_compile_options(network PUBLIC ${COMPILE_OPTIONS})

# 添加包含目录
target_include_directories(network PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# 设置库依赖
target_link_libraries(network
    PRIVATE
    common
    ${MUDUO_NET}
    ${MUDUO_BASE}
) 