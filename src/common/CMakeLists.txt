add_library(common STATIC
    memory_pool.cpp
    thread_pool.cpp
)

target_include_directories(common PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/../include
)

target_compile_options(common PUBLIC
    -Wall -Wextra -Wpedantic -Werror -pthread
) 