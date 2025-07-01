cmake_minimum_required(VERSION 3.14)
project(DistributedLogMonitorClient)
set(CMAKE_AUTOMOC ON)

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

# 输出目录全部基于CMAKE_BINARY_DIR（即build目录）
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 查找必要的依赖
find_package(Threads REQUIRED)
find_package(Protobuf REQUIRED)
find_package(gRPC QUIET)

# 查找ZLIB依赖
find_package(ZLIB REQUIRED)
if(ZLIB_FOUND)
    message(STATUS "找到ZLIB: ${ZLIB_LIBRARIES}")
else()
    message(FATAL_ERROR "无法找到ZLIB库，请确保已安装")
endif()

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
    ${ZLIB_INCLUDE_DIRS}
)

add_subdirectory(src)
# add_subdirectory(examples)
# add_subdirectory(tests)
# add_subdirectory(tools)

# 自动检测Qt5或Qt6
find_package(Qt5 COMPONENTS Widgets Network REQUIRED)
# 如果需要支持Qt6，可加如下行：
# find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

# qt5_wrap_cpp(MOC_SOURCES views/DraggableTableView.h)  # 已不再需要，AUTOMOC自动处理

add_executable(DistributedLogMonitorClient
    main.cpp
    MainWindow.cpp MainWindow.h
    ApiClient.cpp ApiClient.h
    models/LogModel.cpp models/LogModel.h
    models/AlertModel.cpp models/AlertModel.h
    models/RuleModel.cpp models/RuleModel.h
    models/MonitorModel.cpp models/MonitorModel.h
    views/LogView.cpp views/LogView.h
    views/DraggableTableView.cpp views/DraggableTableView.h
    views/AlertView.cpp views/AlertView.h
    views/RuleView.cpp views/RuleView.h
    views/MonitorView.cpp views/MonitorView.h
    views/CollectorView.cpp views/CollectorView.h
    views/CollectorLogModel.cpp views/CollectorLogModel.h
)

# 确保Qt头文件和库被包含和链接
if(TARGET DistributedLogMonitorClient)
    target_link_libraries(DistributedLogMonitorClient PRIVATE Qt5::Widgets Qt5::Network)
    target_include_directories(DistributedLogMonitorClient PRIVATE ${Qt5Widgets_INCLUDE_DIRS} ${Qt5Network_INCLUDE_DIRS})
endif()

add_subdirectory(src/collector) 