cmake_minimum_required(VERSION 3.15)
project(MarketDataEngine VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler warnings
if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Build type
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Google Test
include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
)
FetchContent_MakeAvailable(googletest)

# Domain library
add_library(domain
    src/domain/value_objects/Price.cpp
)

target_include_directories(domain PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Main executable
add_executable(market_data_engine
    src/main.cpp
)

target_include_directories(market_data_engine PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_link_libraries(market_data_engine PRIVATE domain)

# Enable testing
enable_testing()
add_subdirectory(tests)
