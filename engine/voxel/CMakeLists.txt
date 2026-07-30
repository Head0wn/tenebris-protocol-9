add_library(tenebris_voxel STATIC)

add_library(Tenebris::Voxel ALIAS tenebris_voxel)

target_sources(
    tenebris_voxel
    PRIVATE
        src/VoxelChunk.cpp
        src/GreedyMesher.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/tenebris/voxel/VoxelChunk.hpp
            include/tenebris/voxel/GreedyMesher.hpp
)

target_include_directories(
    tenebris_voxel
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
)

target_compile_features(tenebris_voxel PUBLIC cxx_std_20)
tenebris_set_project_warnings(tenebris_voxel)
