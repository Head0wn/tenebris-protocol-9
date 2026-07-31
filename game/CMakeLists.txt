add_executable(tenebris)

target_sources(tenebris PRIVATE src/main.cpp)
target_link_libraries(
    tenebris
    PRIVATE
        Tenebris::Core
        Tenebris::Platform
        Tenebris::Renderer
        Tenebris::Scene
)
target_compile_features(tenebris PRIVATE cxx_std_20)
tenebris_set_project_warnings(tenebris)

set_target_properties(
    tenebris
    PROPERTIES
        OUTPUT_NAME "TENEBRIS"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:tenebris>"
)

set(TENEBRIS_COMPILED_SHADER_DIR "${CMAKE_SOURCE_DIR}/assets/shaders/compiled")
add_custom_command(
    TARGET tenebris
    POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:tenebris>/assets/shaders/compiled"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TENEBRIS_COMPILED_SHADER_DIR}/voxel.vert.spv"
        "$<TARGET_FILE_DIR:tenebris>/assets/shaders/compiled/voxel.vert.spv"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${TENEBRIS_COMPILED_SHADER_DIR}/voxel.frag.spv"
        "$<TARGET_FILE_DIR:tenebris>/assets/shaders/compiled/voxel.frag.spv"
    VERBATIM
)
