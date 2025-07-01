include_directories(${CMAKE_SOURCE_DIR}/include)

add_executable(collector_server collector_server.cpp)
target_link_libraries(collector_server
    collector
    common
    network
    ${MUDUO_NET}
    ${MUDUO_BASE}
    pthread
) 