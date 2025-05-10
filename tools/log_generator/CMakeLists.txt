cmake_minimum_required(VERSION 3.10)
project(log_generator)

# 设置C++标准
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 添加头文件搜索路径
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_SOURCE_DIR}/include
)

# 添加日志生成器库
add_library(log_generator STATIC
    log_generator.cpp
    log_generator.h
)

# 添加示例程序
add_executable(log_generator_example
    log_generator_example.cpp
)

# 添加基准测试程序
add_executable(log_benchmark
    benchmark.cpp
)

# 链接库
target_link_libraries(log_generator
    pthread
)

target_link_libraries(log_generator_example
    log_generator
    network
    pthread
    muduo_net
    muduo_base
)

target_link_libraries(log_benchmark
    log_generator
    network
    pthread
    muduo_net
    muduo_base
)

# 如果是Windows，需要链接winsock2
if(WIN32)
    target_link_libraries(log_generator ws2_32)
    target_link_libraries(log_generator_example ws2_32)
    target_link_libraries(log_benchmark ws2_32)
endif()

# 安装目标
install(TARGETS log_generator log_generator_example log_benchmark
    RUNTIME DESTINATION bin
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
)

# 安装头文件
install(FILES log_generator.h
    DESTINATION include/xumj/tools
) 