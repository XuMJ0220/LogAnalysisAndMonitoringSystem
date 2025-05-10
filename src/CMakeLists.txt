# src/CMakeLists.txt

# 查找需要的库
find_package(ZLIB REQUIRED)

# 添加公共库目标
add_library(common STATIC
    common/memory_pool.cpp
    common/thread_pool.cpp
)

# 添加日志收集器库
add_library(collector STATIC
    collector/log_collector.cpp
)

# 设置库依赖
target_link_libraries(collector
    PRIVATE
    common
    ${ZLIB_LIBRARIES}
)

# 设置编译选项
set(COMPILE_OPTIONS
    -Wall
    -Wextra
    -Wpedantic
    -Werror
    -pthread
)

# 应用编译选项到所有目标
target_compile_options(common PUBLIC ${COMPILE_OPTIONS})
target_compile_options(collector PUBLIC ${COMPILE_OPTIONS})

# 添加包含目录
target_include_directories(common PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

target_include_directories(collector PUBLIC
    ${CMAKE_SOURCE_DIR}/include
)

# 添加子目录
add_subdirectory(network)
add_subdirectory(processor)
add_subdirectory(storage)
add_subdirectory(analyzer)
add_subdirectory(alert) 