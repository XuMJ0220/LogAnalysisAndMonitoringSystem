# src/processor/CMakeLists.txt

# 暂时添加一个空的处理器库，后续会逐步实现
add_library(processor STATIC
    log_processor.cpp
)

# 设置编译选项
set_target_properties(processor PROPERTIES
    CXX_STANDARD 17
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
    ${CMAKE_SOURCE_DIR}
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