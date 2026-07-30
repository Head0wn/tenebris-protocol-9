add_library(tenebris_scene STATIC)

add_library(Tenebris::Scene ALIAS tenebris_scene)

target_sources(
    tenebris_scene
    PRIVATE
        src/RenderScene.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/tenebris/scene/Math.hpp
            include/tenebris/scene/RenderScene.hpp
)

target_include_directories(
    tenebris_scene
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_link_libraries(
    tenebris_scene
    PUBLIC
        Tenebris::Voxel
)

target_compile_features(tenebris_scene PUBLIC cxx_std_20)
tenebris_set_project_warnings(tenebris_scene)
