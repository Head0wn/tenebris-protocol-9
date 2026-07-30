add_library(tenebris_core STATIC)

add_library(Tenebris::Core ALIAS tenebris_core)

target_sources(
    tenebris_core
    PRIVATE
        src/Application.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/tenebris/core/Application.hpp
)

target_include_directories(
    tenebris_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(tenebris_core PUBLIC cxx_std_20)
tenebris_set_project_warnings(tenebris_core)
