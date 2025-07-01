# src/processor/CMakeLists.txt

# 添加处理器库
add_library(processor STATIC
    log_processor.cpp
)

# 设置编译选项
set_target_properties(processor PROPERTIES
    CXX_STANDARD 20  # 升级到C++20以支持starts_with等新特性
    CXX_STANDARD_REQUIRED ON
)

# 添加警告选项
target_compile_options(processor PRIVATE -Wall -Wextra)

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

# 查找zlib
find_package(ZLIB REQUIRED)

# 查找UUID库
find_library(UUID_LIBRARY NAMES uuid)
if(NOT UUID_LIBRARY)
    message(FATAL_ERROR "UUID library not found")
endif()

# 添加包含目录
target_include_directories(processor PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 链接依赖库
target_link_libraries(processor
    common
    storage
    network
    analyzer
    nlohmann_json::nlohmann_json
    ${ZLIB_LIBRARIES}
    ${UUID_LIBRARY}
)

# 安装配置
install(TARGETS processor
    ARCHIVE DESTINATION lib
    LIBRARY DESTINATION lib
    RUNTIME DESTINATION bin
)

# 添加processor_server可执行文件
message(STATUS "正在添加processor_server目标")
add_executable(processor_server processor_server.cpp)
message(STATUS "已添加processor_server目标")
target_link_libraries(processor_server
    processor
    common
    storage
    network
    analyzer
    nlohmann_json::nlohmann_json
    ${ZLIB_LIBRARIES}
    ${UUID_LIBRARY}
    pthread
)
install(TARGETS processor_server
    RUNTIME DESTINATION bin
) 