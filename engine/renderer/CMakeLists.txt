add_library(tenebris_renderer STATIC)

add_library(Tenebris::Renderer ALIAS tenebris_renderer)

target_sources(
    tenebris_renderer
    PRIVATE
        src/RendererSystemV2.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/tenebris/renderer/RendererSystem.hpp
)

target_include_directories(
    tenebris_renderer
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_definitions(tenebris_renderer PRIVATE VK_NO_PROTOTYPES)
target_link_libraries(
    tenebris_renderer
    PRIVATE
        SDL3::SDL3-static
        Vulkan::Headers
        Tenebris::Scene
)
target_compile_features(tenebris_renderer PUBLIC cxx_std_20)
tenebris_set_project_warnings(tenebris_renderer)

# Vulkan function-pointer loading and explicit ABI boundaries are intentional.
# C4324 is expected because the GPU push-constant payload is deliberately alignas(16).
if(MSVC)
    target_compile_options(tenebris_renderer PRIVATE /wd4834 /wd4324)
else()
    target_compile_options(tenebris_renderer PRIVATE -Wno-unused-result -Wno-sign-conversion)
endif()
