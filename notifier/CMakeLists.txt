cmake_minimum_required(VERSION 3.20)
project(notifier VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Werror")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -DNDEBUG")

find_package(nlohmann_json CONFIG REQUIRED)
find_package(fmt CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(redis++ CONFIG REQUIRED)
find_package(hiredis CONFIG REQUIRED)
find_package(libpqxx CONFIG REQUIRED)
find_package(httplib CONFIG REQUIRED)

set(SOURCES
    src/main.cpp
    src/config.cpp
    src/redis_bus.cpp
    src/formatter.cpp
    src/throttle.cpp
    src/mute_state.cpp
    src/dedup.cpp
    src/audit.cpp
    src/health.cpp
    src/util.cpp
)

add_executable(notifier ${SOURCES})

target_include_directories(notifier PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_link_libraries(notifier PRIVATE
    nlohmann_json::nlohmann_json
    fmt::fmt
    spdlog::spdlog
    redis++::redis++
    hiredis::hiredis
    libpqxx::pqxx
    httplib::httplib
)

option(BUILD_TESTS "Build tests" ON)
if(BUILD_TESTS)
    find_package(Catch2 3 CONFIG REQUIRED)
    enable_testing()
    
    add_executable(notifier_tests
        tests/test_formatter.cpp
        tests/test_dedup.cpp
        tests/test_throttle.cpp
        tests/test_mute.cpp
        src/formatter.cpp
        src/throttle.cpp
        src/mute_state.cpp
        src/dedup.cpp
        src/util.cpp
    )
    
    target_include_directories(notifier_tests PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
    )
    
    target_link_libraries(notifier_tests PRIVATE
        Catch2::Catch2WithMain
        nlohmann_json::nlohmann_json
        fmt::fmt
        spdlog::spdlog
    )
    
    include(CTest)
    include(Catch)
    catch_discover_tests(notifier_tests)
endif()

install(TARGETS notifier DESTINATION bin)