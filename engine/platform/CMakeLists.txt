add_library(tenebris_platform STATIC)

add_library(Tenebris::Platform ALIAS tenebris_platform)

target_sources(
    tenebris_platform
    PRIVATE
        src/PlatformSystem.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/tenebris/platform/PlatformSystem.hpp
)

target_include_directories(
    tenebris_platform
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(
    tenebris_platform
    PRIVATE
        SDL3::SDL3-static
        Vulkan::Headers
)
target_compile_features(tenebris_platform PUBLIC cxx_std_20)
tenebris_set_project_warnings(tenebris_platform)
