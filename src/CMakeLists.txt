# src/CMakeLists.txt

# 查找需要的库
find_package(ZLIB REQUIRED)

# 添加子目录
add_subdirectory(common)
add_subdirectory(network)
add_subdirectory(storage)
add_subdirectory(analyzer)
add_subdirectory(alert)
# add_subdirectory(grpc)
add_subdirectory(processor)
add_subdirectory(collector) 