# src/storage/CMakeLists.txt

# 暂时添加一个空的存储库，后续会逐步实现
add_library(storage STATIC
    redis_storage.cpp
    mysql_storage.cpp
    storage_factory.cpp
)

set_target_properties(storage PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

target_compile_options(storage PRIVATE
    -Wall
    -Wextra
    -Wpedantic
)

target_include_directories(storage PRIVATE
    ${PROJECT_SOURCE_DIR}/include
)

# 查找nlohmann/json库
find_package(nlohmann_json QUIET)
if (NOT nlohmann_json_FOUND)
    include(FetchContent)
    FetchContent_Declare(
        json
        URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
    )
    FetchContent_MakeAvailable(json)
endif()

# 链接依赖库
target_link_libraries(storage
    common
    ${REDIS_LIBRARIES}
    ${MYSQL_LIBRARIES}
    uuid
    nlohmann_json::nlohmann_json
) 