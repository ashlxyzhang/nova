find_package(GTest CONFIG REQUIRED)

set(NOVA_TEST_SOURCES
    unit/event_data_test.cc
    unit/scrubber_state_test.cc
    integration/reader_to_event_data_test.cc
)

add_executable(nova_tests ${NOVA_TEST_SOURCES})

target_compile_features(nova_tests PRIVATE cxx_std_23)
set_target_properties(nova_tests PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(nova_tests PRIVATE
    "${CMAKE_SOURCE_DIR}/src"
    "${CMAKE_SOURCE_DIR}/resources"
    "${CMAKE_SOURCE_DIR}/external/imgui"
)

# Tests depend on header-only EventData + DVEventReader + Scrubber::State.
# Link the minimum external libs those pull in.
target_link_libraries(nova_tests PRIVATE
    GTest::gtest_main
    glm::glm
    dv::processing
    Boost::iostreams
    SDL3::SDL3          # Scrubber.hh transitively includes SDL GPU headers
    opencv_core
    opencv_imgproc
    opencv_highgui
)

if(APPLE)
    target_compile_definitions(nova_tests PRIVATE BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)
endif()

include(GoogleTest)
gtest_discover_tests(nova_tests
    WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
    PROPERTIES ENVIRONMENT "NOVA_TEST_FIXTURES=${CMAKE_CURRENT_SOURCE_DIR}/fixtures"
)
