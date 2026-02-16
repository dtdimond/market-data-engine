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

# IXWebSocket (for WebSocket tools)
FetchContent_Declare(
    ixwebsocket
    GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
    GIT_TAG v11.4.5
)
set(USE_TLS ON CACHE BOOL "" FORCE)
set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(ixwebsocket)

# JSON library
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.11.3
)
FetchContent_MakeAvailable(json)

# Domain library
add_library(domain
    src/domain/value_objects/Price.cpp
    src/domain/value_objects/Quantity.cpp
    src/domain/value_objects/Timestamp.cpp
    src/domain/value_objects/MarketAsset.cpp
    src/domain/value_objects/PriceLevel.cpp
    src/domain/aggregates/OrderBook.cpp
)

target_include_directories(domain PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

# Infrastructure library
add_library(infrastructure
    src/infrastructure/PolymarketMessageParser.cpp
)

target_link_libraries(infrastructure PUBLIC domain PRIVATE nlohmann_json::nlohmann_json)

target_include_directories(infrastructure PUBLIC
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

# Tools
add_executable(ws_listener tools/ws_listener.cpp)
target_link_libraries(ws_listener PRIVATE ixwebsocket)

# Enable testing
enable_testing()
add_subdirectory(tests)
