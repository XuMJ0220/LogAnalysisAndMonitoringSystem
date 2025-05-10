# tests/CMakeLists.txt

cmake_minimum_required(VERSION 3.10)

# 手动指定GTest和GMock库的路径
set(GTEST_ROOT /usr/local/lib/gtest)
set(GMOCK_ROOT /usr/local/lib/gmock)

# 手动设置GTest库
set(GTEST_INCLUDE_DIRS /usr/src/googletest/googletest/include)
set(GTEST_LIBRARIES /usr/lib/libgtest.a /usr/lib/libgtest_main.a)

# 手动设置GMock库
set(GMOCK_INCLUDE_DIRS /usr/src/googletest/googlemock/include)
set(GMOCK_LIBRARIES /usr/lib/libgmock.a /usr/lib/libgmock_main.a)

# 添加测试可执行文件
set(TEST_SOURCES
    main_test.cpp
    test_log_collector.cpp
    test_alert_manager.cpp
    test_analyzer_rules.cpp
    test_log_processor.cpp
    test_storage.cpp
)

# 创建测试可执行文件
add_executable(run_tests ${TEST_SOURCES})

# 设置编译选项
target_compile_features(run_tests PRIVATE cxx_std_17)
set_target_properties(run_tests PROPERTIES CXX_EXTENSIONS OFF)

# 添加编译警告
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(run_tests PRIVATE -Wall -Wextra -Wpedantic -Werror)
elseif(MSVC)
    target_compile_options(run_tests PRIVATE /W4 /WX)
endif()

# 包含头文件目录
target_include_directories(run_tests PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)

# 链接测试库和项目库
target_link_libraries(run_tests PRIVATE
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
    pthread
    collector
    processor
    analyzer
    storage
    alert
    network
    common
)

# 添加测试
include(GoogleTest)
gtest_discover_tests(run_tests)

# 添加单独的测试可执行文件（可选）
add_executable(test_collector test_log_collector.cpp)
target_compile_features(test_collector PRIVATE cxx_std_17)
target_include_directories(test_collector PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)
target_link_libraries(test_collector PRIVATE
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
    pthread
    collector
    common
    storage
    network
    analyzer
    alert
    ${ZLIB_LIBRARIES}
)

add_executable(test_alert test_alert_manager.cpp)
target_compile_features(test_alert PRIVATE cxx_std_17)
target_include_directories(test_alert PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)
target_link_libraries(test_alert PRIVATE
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
    pthread
    alert
    analyzer
    common
)

add_executable(test_analyzer test_analyzer_rules.cpp)
target_compile_features(test_analyzer PRIVATE cxx_std_17)
target_include_directories(test_analyzer PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)
target_link_libraries(test_analyzer PRIVATE
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
    pthread
    analyzer
    common
)

add_executable(test_processor test_log_processor.cpp)
target_compile_features(test_processor PRIVATE cxx_std_17)
target_include_directories(test_processor PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)
target_link_libraries(test_processor PRIVATE
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
    pthread
    processor
    common
)

add_executable(test_storage test_storage.cpp)
target_compile_features(test_storage PRIVATE cxx_std_17)
target_include_directories(test_storage PRIVATE 
    ${CMAKE_SOURCE_DIR}/include
    ${GTEST_INCLUDE_DIRS}
    ${GMOCK_INCLUDE_DIRS}
)
target_link_libraries(test_storage PRIVATE
    ${GTEST_LIBRARIES}
    ${GMOCK_LIBRARIES}
    pthread
    storage
    common
) 