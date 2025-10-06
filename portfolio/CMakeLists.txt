cmake_minimum_required(VERSION 3.20)
project(portfolio VERSION 1.0.0 LANGUAGES CXX)

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
    src/redis_bus.cpp
    src/rpc_solana.cpp
    src/price_oracle.cpp
    src/cg_client.cpp
    src/dex_client.cpp
    src/jupiter_client.cpp
    src/metadata_cache.cpp
    src/valuation.cpp
    src/postgres_store.cpp
    src/health.cpp
    src/util.cpp
)

add_executable(portfolio ${SOURCES})

target_include_directories(portfolio PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(portfolio PRIVATE
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
    
    add_executable(portfolio_tests
        tests/test_valuation.cpp
        tests/test_wallet_cmds.cpp
        src/valuation.cpp
        src/util.cpp
    )
    
    target_include_directories(portfolio_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(portfolio_tests PRIVATE
        Catch2::Catch2WithMain
        nlohmann_json::nlohmann_json
        fmt::fmt
        spdlog::spdlog
    )
    
    include(CTest)
    include(Catch)
    catch_discover_tests(portfolio_tests)
endif()

install(TARGETS portfolio DESTINATION bin)