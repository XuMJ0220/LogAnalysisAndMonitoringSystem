cmake_minimum_required(VERSION 3.10)
project(collector_file_watch_example)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(collector_file_watch_example
    collector_file_watch_example.cpp
    ../../../src/collector/log_collector.cpp
    ../../../src/common/memory_pool.cpp
    ../../../src/common/thread_pool.cpp
)

target_include_directories(collector_file_watch_example PRIVATE
    /home/xumj/项目/Distributed-Real-time-Log-Analysis-and-Monitoring-System/include
)

target_link_libraries(collector_file_watch_example
    log4cpp
    pthread
    z
) 