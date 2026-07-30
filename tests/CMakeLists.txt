add_executable(tenebris_core_tests ApplicationTests.cpp)

target_link_libraries(tenebris_core_tests PRIVATE Tenebris::Core)
target_compile_features(tenebris_core_tests PRIVATE cxx_std_20)
tenebris_set_project_warnings(tenebris_core_tests)

add_test(NAME tenebris.core.lifecycle COMMAND tenebris_core_tests)
add_test(NAME tenebris.game.smoke COMMAND tenebris --headless-smoke-test)
