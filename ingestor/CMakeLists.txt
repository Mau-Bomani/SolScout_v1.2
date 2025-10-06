cmake_minimum_required(VERSION 3.20)
project(ingestor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

# Find packages
find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(CURL REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

set(SOURCES
    src/main.cpp
    src/config.cpp
    src/http_client.cpp
    src/rpc_clients/raydium_client.cpp
    src/rpc_clients/orca_client.cpp
    src/rpc_clients/jupiter_client.cpp
    src/rpc_clients/solana_rpc_client.cpp
    src/normalize.cpp
    src/bar_synth.cpp
    src/impact_model.cpp
    src/store_pg.cpp
    src/redis_bus.cpp
    src/health.cpp
    src/util.cpp
)

add_executable(ingestor ${SOURCES})

target_include_directories(ingestor PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(ingestor PRIVATE
    nlohmann_json::nlohmann_json
    fmt::fmt
    spdlog::spdlog
    redis++::redis++
    hiredis::hiredis
    CURL::libcurl
    libpqxx::pqxx
    httplib::httplib
)

# Testing
option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    find_package(Catch2 3 CONFIG REQUIRED)
    enable_testing()
    
    add_executable(ingestor_tests
        tests/test_bar_synth.cpp
        tests/test_impact_model.cpp
        tests/test_normalize.cpp
        src/bar_synth.cpp
        src/impact_model.cpp
        src/normalize.cpp
        src/util.cpp
    )
    
    target_include_directories(ingestor_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(ingestor_tests PRIVATE
        Catch2::Catch2WithMain
        nlohmann_json::nlohmann_json
        fmt::fmt
        spdlog::spdlog
    )
    
    include(CTest)
    include(Catch)
    catch_discover_tests(ingestor_tests)
endif()

install(TARGETS ingestor DESTINATION bin)