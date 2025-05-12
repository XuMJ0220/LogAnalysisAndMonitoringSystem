# 集成测试CMakeLists.txt
cmake_minimum_required(VERSION 3.10)

# 添加日志处理集成测试
add_executable(log_processing_integration_test log_processing_integration_test.cpp)
target_link_libraries(log_processing_integration_test
    xumj_network
    xumj_processor
    xumj_storage
    xumj_common
    ${MYSQL_LIBRARIES}
    ${CMAKE_THREAD_LIBS_INIT}
)

# 安装测试程序
install(TARGETS log_processing_integration_test DESTINATION bin/tests) 