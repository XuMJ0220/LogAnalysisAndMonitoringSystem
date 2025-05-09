cmake_minimum_required(VERSION 3.10)
project(LogAnalysisSystem VERSION 1.0)

# 设置C++标准为C++17
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 编译选项
set(COMPILE_OPTIONS
    -Wall
    -Wextra
    -Wpedantic
    -O2
)

# 设置输出目录
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 查找必要的依赖
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC QUIET)

# 手动设置gRPC路径
if(NOT gRPC_FOUND)
    set(GRPC_CPP_PLUGIN_EXECUTABLE /usr/local/bin/grpc_cpp_plugin)
endif()

# 创建proto_gen目录
file(MAKE_DIRECTORY ${CMAKE_BINARY_DIR}/proto_gen)

# 注意：我们已经手动生成了proto文件，所以这里不再需要生成它们
# 但我们仍然需要添加一个自定义目标，以便其他目标可以依赖它
add_custom_target(proto_gen)

# 查找Redis依赖
find_path(HIREDIS_INCLUDE_DIR hiredis/hiredis.h)
find_library(HIREDIS_LIBRARY hiredis)
if(NOT HIREDIS_INCLUDE_DIR OR NOT HIREDIS_LIBRARY)
    message(FATAL_ERROR "无法找到hiredis库，请确保已安装")
endif()
set(REDIS_INCLUDE_DIRS ${HIREDIS_INCLUDE_DIR})
set(REDIS_LIBRARIES ${HIREDIS_LIBRARY})
message(STATUS "找到Redis: ${REDIS_LIBRARIES}")

# 查找MySQL依赖
find_path(MYSQL_INCLUDE_DIR mysql/mysql.h
    /usr/include/mysql
    /usr/local/include/mysql
    /usr/mysql/include/mysql
)
find_library(MYSQL_LIBRARY mysqlclient
    /usr/lib
    /usr/lib/mysql
    /usr/local/lib
    /usr/local/lib/mysql
    /usr/mysql/lib
)
if(NOT MYSQL_INCLUDE_DIR OR NOT MYSQL_LIBRARY)
    message(FATAL_ERROR "无法找到MySQL库，请确保已安装")
endif()
set(MYSQL_INCLUDE_DIRS ${MYSQL_INCLUDE_DIR})
set(MYSQL_LIBRARIES ${MYSQL_LIBRARY})
message(STATUS "找到MySQL: ${MYSQL_LIBRARIES}")

# 查找Muduo依赖
find_library(MUDUO_BASE muduo_base)
find_library(MUDUO_NET muduo_net)
if(NOT MUDUO_BASE OR NOT MUDUO_NET)
    message(FATAL_ERROR "无法找到muduo库，请确保已安装")
endif()
message(STATUS "找到Muduo: ${MUDUO_BASE} ${MUDUO_NET}")

# 查找UUID依赖
find_library(UUID_LIBRARY uuid)
if(NOT UUID_LIBRARY)
    message(FATAL_ERROR "无法找到uuid库，请确保已安装libuuid")
endif()
message(STATUS "找到UUID: ${UUID_LIBRARY}")

# 包含目录
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${CMAKE_BINARY_DIR}/proto_gen  # 添加proto生成目录
    ${PROTOBUF_INCLUDE_DIRS}
    ${GRPC_INCLUDE_DIRS}
    ${REDIS_INCLUDE_DIRS}
    ${MYSQL_INCLUDE_DIRS}
)

# 添加子目录
add_subdirectory(src)
add_subdirectory(examples)
add_subdirectory(tests)

# 安装规则
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
    DESTINATION include/log_analysis_system
    FILES_MATCHING PATTERN "*.h"
) 