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
    log_collector_example.cpp
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

# 网络通信示例
add_executable(network_example
    network_example.cpp
)

# 设置编译选项
target_compile_options(network_example PRIVATE -Wall -Wextra)

# 添加包含目录
target_include_directories(network_example PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(network_example
    network
    common
    muduo_net
    muduo_base
    pthread
)

# 存储示例
add_executable(storage_example
    storage_example.cpp
)

# 设置编译选项
target_compile_options(storage_example PRIVATE -Wall -Wextra)

# 添加包含目录
target_include_directories(storage_example PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(storage_example
    storage
    common
    nlohmann_json::nlohmann_json
    ${REDIS_LIBRARIES}
    ${MYSQL_LIBRARIES}
)

# 存储工厂示例
add_executable(storage_factory_example
    storage_factory_example.cpp
)

# 设置编译选项
target_compile_options(storage_factory_example PRIVATE -Wall -Wextra)

# 添加包含目录
target_include_directories(storage_factory_example PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(storage_factory_example
    storage
    common
    nlohmann_json::nlohmann_json
    ${REDIS_LIBRARIES}
    ${MYSQL_LIBRARIES}
)

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

# 日志处理器示例
add_executable(processor_example
    processor_example.cpp
)

# 设置编译选项
target_compile_options(processor_example PRIVATE -Wall -Wextra)

# 添加包含目录
target_include_directories(processor_example PRIVATE
    ${CMAKE_SOURCE_DIR}
)

# 链接依赖库
target_link_libraries(processor_example
    processor
    analyzer
    storage
    network
    common
    nlohmann_json::nlohmann_json
    ${ZLIB_LIBRARIES}
    ${UUID_LIBRARY}
    pthread
)

# 告警管理器示例
add_executable(alert_example
    alert_example.cpp
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

# 如果有其他示例，可以在这里添加 