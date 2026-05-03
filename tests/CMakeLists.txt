find_package(GTest CONFIG REQUIRED)

set(NOVA_TEST_SOURCES
    unit/event_data_test.cc
    unit/scrubber_state_test.cc
    integration/reader_to_event_data_test.cc
    integration/api_test.cc
)

add_executable(nova_tests ${NOVA_TEST_SOURCES})

target_compile_features(nova_tests PRIVATE cxx_std_23)
set_target_properties(nova_tests PROPERTIES CXX_EXTENSIONS OFF)

# Link against the nova_lib library which contains all implementations
target_link_libraries(nova_tests PRIVATE
    GTest::gtest_main
    nova_lib          # This provides DataSource, GPUDevice, Window, etc.
)

include(GoogleTest)
gtest_discover_tests(nova_tests
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    PROPERTIES ENVIRONMENT "NOVA_TEST_FIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)