add_library(collector STATIC log_collector.cpp)
target_include_directories(collector PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../include)
target_link_libraries(collector PUBLIC ${ZLIB_LIBRARIES})

add_executable(collector_server collector_server.cpp)
target_link_libraries(collector_server
    collector
    common
    network
    ${MUDUO_NET}
    ${MUDUO_BASE}
    pthread
) 