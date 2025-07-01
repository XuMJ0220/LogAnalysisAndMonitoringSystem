# src/analyzer/CMakeLists.txt

# 添加静态库：analyzer
add_library(analyzer STATIC
    log_analyzer.cpp
)

# 设置编译选项
set_target_properties(analyzer PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# 添加警告选项
target_compile_options(analyzer PRIVATE -Wall -Wextra)

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

# 添加包含目录
target_include_directories(analyzer PRIVATE 
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

# 链接依赖库
target_link_libraries(analyzer
    common
    storage
    nlohmann_json::nlohmann_json
) 