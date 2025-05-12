# 解析器性能测试CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 添加解析器基准测试
add_executable(parser_benchmark parser_benchmark.cpp)
target_link_libraries(parser_benchmark
    xumj_processor
    xumj_common
    ${CMAKE_THREAD_LIBS_INIT}
)

# 安装测试程序
install(TARGETS parser_benchmark DESTINATION bin/tests) 