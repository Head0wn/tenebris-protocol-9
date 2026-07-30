add_executable(tenebris)

target_sources(tenebris PRIVATE src/main.cpp)
target_link_libraries(tenebris PRIVATE Tenebris::Core)
target_compile_features(tenebris PRIVATE cxx_std_20)
tenebris_set_project_warnings(tenebris)

set_target_properties(
    tenebris
    PROPERTIES
        OUTPUT_NAME "TENEBRIS"
        VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:tenebris>"
)
