# examples/CMakeLists.txt

# 查找必要的依赖库
find_package(nlohmann_json QUIET)
if(NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.10.5/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
endif()

# 日志收集器示例
add_executable(log_collector_example
    legacy/log_collector_example.cpp
)

# 设置编译选项
target_compile_options(log_collector_example PRIVATE -Wall -Wextra)

# 添加包含目录
target_include_directories(log_collector_example PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(log_collector_example
    collector
    common
    ${ZLIB_LIBRARIES}
    pthread
    -lz
)

# 增强版日志收集器示例
add_executable(collector_enhanced_example
    collector_enhanced_example.cpp
)

# 设置编译选项
target_compile_options(collector_enhanced_example PRIVATE -Wall -Wextra)

# 添加包含目录
target_include_directories(collector_enhanced_example PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(collector_enhanced_example
    collector
    common
    ${ZLIB_LIBRARIES}
    pthread
    -lz
)

# 存储工厂示例
add_executable(storage_factory_example storage_factory_example.cpp)
target_compile_options(storage_factory_example PRIVATE -Wall -Wextra)
target_include_directories(storage_factory_example PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(storage_factory_example storage common pthread ${REDIS_LIBRARIES} ${MYSQL_LIBRARIES} uuid nlohmann_json::nlohmann_json)
set_target_properties(storage_factory_example PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 日志分析器示例
add_executable(analyzer_example
    analyzer_example.cpp
)

# 设置编译选项
target_compile_options(analyzer_example PRIVATE -Wall -Wextra)

# 添加包含目录
target_include_directories(analyzer_example PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(analyzer_example
    analyzer
    common
    storage
    nlohmann_json::nlohmann_json
    pthread
)

# 设置输出目录
set_target_properties(analyzer_example PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 安装配置
install(TARGETS analyzer_example
    RUNTIME DESTINATION bin/examples
)

# 日志处理器示例
add_executable(processor_example processor_example.cpp)
target_link_libraries(processor_example processor storage analyzer common network pthread muduo_net muduo_base)
set_target_properties(processor_example PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 安装配置
install(TARGETS processor_example
    RUNTIME DESTINATION bin/examples
)

# 告警管理器示例
add_executable(alert_example
    legacy/alert_example.cpp
)

# 设置编译选项
target_compile_options(alert_example PRIVATE -Wall -Wextra)

# 添加包含目录
target_include_directories(alert_example PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(alert_example
    alert
    analyzer
    storage
    network
    common
    nlohmann_json::nlohmann_json
    ${CURL_LIBRARIES}
    ${UUID_LIBRARY}
    pthread
)

# 完整系统示例
add_executable(complete_system_example
    complete_system_example.cpp
)

# 设置编译选项
target_compile_options(complete_system_example PRIVATE -Wall -Wextra)

# 添加包含目录
target_include_directories(complete_system_example PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(complete_system_example
    collector
    processor
    analyzer
    storage
    alert
    network
    common
    nlohmann_json::nlohmann_json
    ${ZLIB_LIBRARIES}
    ${REDIS_LIBRARIES}
    ${MYSQL_LIBRARIES}
    ${CURL_LIBRARIES}
    ${UUID_LIBRARY}
    muduo_net
    muduo_base
    pthread
)

# 网络服务器示例
add_executable(network_server_example
    network_server_example.cpp
)

# 设置编译选项
target_compile_options(network_server_example PRIVATE -Wall -Wextra)

# 添加包含目录
target_include_directories(network_server_example PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(network_server_example
    network
    common
    pthread
)

# 网络客户端示例
add_executable(network_client_example
    network_client_example.cpp
)

# 设置编译选项
target_compile_options(network_client_example PRIVATE -Wall -Wextra)

# 添加包含目录
target_include_directories(network_client_example PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(network_client_example
    network
    common
    pthread
)

# Redis存储示例
add_executable(redis_storage_example redis_storage_example.cpp)
target_compile_options(redis_storage_example PRIVATE -Wall -Wextra)
target_include_directories(redis_storage_example PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(redis_storage_example storage common pthread ${REDIS_LIBRARIES})
set_target_properties(redis_storage_example PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# MySQL存储示例
add_executable(mysql_storage_example mysql_storage_example.cpp)
target_compile_options(mysql_storage_example PRIVATE -Wall -Wextra)
target_include_directories(mysql_storage_example PRIVATE ${CMAKE_SOURCE_DIR})
target_link_libraries(mysql_storage_example storage common pthread ${MYSQL_LIBRARIES} uuid)
set_target_properties(mysql_storage_example PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

# 安装所有存储相关示例
install(TARGETS redis_storage_example mysql_storage_example storage_factory_example
    RUNTIME DESTINATION bin/examples
) 