cmake_minimum_required(VERSION 3.28)

project(
    TenebrisProtocol9
    VERSION 0.1.0
    DESCRIPTION "TENEBRIS — Le Protocole 9"
    LANGUAGES CXX
)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TENEBRIS_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(TENEBRIS_BUILD_TESTS "Build TENEBRIS automated tests" ON)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(CompilerWarnings)

add_subdirectory(engine/core)
add_subdirectory(game)

if(TENEBRIS_BUILD_TESTS)
    include(CTest)
    enable_testing()
    add_subdirectory(tests)
endif()
