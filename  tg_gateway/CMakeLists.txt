cmake_minimum_required(VERSION 3.20)
project(tg_gateway VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find packages via vcpkg
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(httplib CONFIG REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/config.cpp
    src/telegram_client.cpp
    src/webhook_server.cpp
    src/poller.cpp
    src/redis_bus.cpp
    src/parser.cpp
    src/rate_limiter.cpp
    src/auth.cpp
    src/health.cpp
    src/util.cpp
)

# Main executable
add_executable(tg_gateway ${SOURCES})

target_include_directories(tg_gateway PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(tg_gateway PRIVATE
    nlohmann_json::nlohmann_json
    fmt::fmt
    spdlog::spdlog
    redis++::redis++
    hiredis::hiredis
    CURL::libcurl
    httplib::httplib
)

# Testing
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    find_package(Catch2 3 CONFIG REQUIRED)
    enable_testing()
    
    add_executable(tg_gateway_tests
        tests/test_parser.cpp
        tests/test_auth.cpp
        tests/test_rate_limiter.cpp
        src/parser.cpp
        src/auth.cpp
        src/rate_limiter.cpp
        src/util.cpp
    )
    
    target_include_directories(tg_gateway_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(tg_gateway_tests PRIVATE
        Catch2::Catch2WithMain
        nlohmann_json::nlohmann_json
        fmt::fmt
        spdlog::spdlog
        redis++::redis++
        hiredis::hiredis
    )
    
    include(CTest)
    include(Catch)
    catch_discover_tests(tg_gateway_tests)
endif()

# Install
install(TARGETS tg_gateway DESTINATION bin)
