cmake_minimum_required(VERSION 3.10)
project(log_generator)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(log4cpp_log_producer log4cpp_log_producer.cpp
    ../CPP-Faker/src/faker/Address.cpp
    ../CPP-Faker/src/faker/Base.cpp
    ../CPP-Faker/src/faker/Company.cpp
    ../CPP-Faker/src/faker/Data.cpp
    ../CPP-Faker/src/faker/ESports.cpp
    ../CPP-Faker/src/faker/JSON_Serializable.cpp
    ../CPP-Faker/src/faker/Name.cpp
    ../CPP-Faker/src/faker/Number.cpp
    ../CPP-Faker/src/faker/PhoneNumber.cpp
    ../CPP-Faker/src/faker/StringVector.cpp
)
target_include_directories(log4cpp_log_producer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
target_link_libraries(log4cpp_log_producer log4cpp)