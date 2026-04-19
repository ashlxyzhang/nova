cmake_minimum_required(VERSION 3.26)
include(FetchContent)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Ceres adds a compile flag "/Zc:__cplusplus" that clang++ cannot handle, so need to do clang-cl for Windows
if(WIN32)
    set(CMAKE_CXX_COMPILER "clang-cl")
    set(CMAKE_C_COMPILER "clang-cl")
endif()

project(NOVA LANGUAGES C CXX)

file(GLOB_RECURSE SOURCES 
    "src/*.cc"
    "src/SLAM/*.cpp"
)
file(GLOB_RECURSE HEADERS 
    "src/*.hh"
    "src/SLAM/*.h"
)

find_package(Vulkan REQUIRED)
find_package(SDL3 REQUIRED)
find_package(SDL3_ttf REQUIRED)
find_package(glm REQUIRED)
find_package(dv-processing REQUIRED)
find_package(Boost COMPONENTS iostreams REQUIRED)
find_package(MetavisionSDK REQUIRED COMPONENTS base core stream)
find_package(MetavisionHAL REQUIRED)
# SLAM
find_package(Eigen3 3.4.1 EXACT REQUIRED)
find_package(Ceres 1.14.0 EXACT REQUIRED) # For solving the optimization problems
find_package(PCL 1.11 COMPONENTS common REQUIRED) # For visualization + storing the 3D poing cloud
FetchContent_Declare( # For camera/parameter configurations
  yaml-cpp
  GIT_REPOSITORY https://github.com/jbeder/yaml-cpp.git
  GIT_TAG yaml-cpp-0.9.0
)
FetchContent_MakeAvailable(yaml-cpp)
find_package(TBB REQUIRED) # For hyperthreading to speed up some computations

# --- Shader compilation ---
find_program(GLSLANG glslang REQUIRED)

set(SHADER_DIR "${CMAKE_SOURCE_DIR}/resources/shaders")
set(COMPILED_SHADER_HEADERS "")

function(compile_shader SRC VAR)
    get_filename_component(DIR "${SRC}" DIRECTORY)
    set(HDR "${DIR}/${VAR}.h")
    add_custom_command(
        OUTPUT  "${HDR}"
        COMMAND "${GLSLANG}" --target-env vulkan1.0 --vn "${VAR}" -o "${HDR}" "${SRC}"
        DEPENDS "${SRC}"
        COMMENT "Compiling ${SRC}"
        VERBATIM
    )
    set(COMPILED_SHADER_HEADERS "${COMPILED_SHADER_HEADERS};${HDR}" PARENT_SCOPE)
endfunction()

file(GLOB_RECURSE SHADER_SOURCES "${SHADER_DIR}/*.comp" "${SHADER_DIR}/*.vert" "${SHADER_DIR}/*.frag")
foreach(SRC ${SHADER_SOURCES})
    get_filename_component(NAME_WE "${SRC}" NAME_WE)
    get_filename_component(EXT "${SRC}" LAST_EXT)
    string(SUBSTRING "${EXT}" 1 -1 EXT_CLEAN)
    compile_shader("${SRC}" "${NAME_WE}_${EXT_CLEAN}")
endforeach()

add_custom_target(shaders DEPENDS ${COMPILED_SHADER_HEADERS})
# ---

set(IMGUI_DIR "${CMAKE_SOURCE_DIR}/external/imgui")
file(GLOB IMGUI_CORE_SOURCES
    "${IMGUI_DIR}/imgui.cpp"
    "${IMGUI_DIR}/imgui_draw.cpp"
    "${IMGUI_DIR}/imgui_tables.cpp"
    "${IMGUI_DIR}/imgui_widgets.cpp"
    "${IMGUI_DIR}/imgui_demo.cpp"
)
file(GLOB IMGUI_BACKENDS
    "${IMGUI_DIR}/backends/imgui_impl_sdl3.cpp"
    "${IMGUI_DIR}/backends/imgui_impl_sdlgpu3.cpp"
)
list(APPEND SOURCES ${IMGUI_CORE_SOURCES} ${IMGUI_BACKENDS})

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS} ${COMPILED_SHADER_HEADERS})
add_dependencies(${CMAKE_PROJECT_NAME} shaders)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)
set_target_properties(${PROJECT_NAME} PROPERTIES CXX_EXTENSIONS OFF)

target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE "resources/")
target_include_directories(${PROJECT_NAME} PRIVATE "src/")
target_include_directories(${PROJECT_NAME} PRIVATE "src/SLAM/esvo2_core/include/")
target_include_directories(${PROJECT_NAME} PRIVATE "src/SLAM/image_representation/include/")
target_include_directories(${PROJECT_NAME} PRIVATE "src/SLAM/minkindr/minkindr/include/")
target_include_directories(${PROJECT_NAME} PRIVATE "src/SLAM/refactor_files/include/")

if(APPLE)
    target_compile_definitions(${PROJECT_NAME} PRIVATE BOOST_STACKTRACE_GNU_SOURCE_NOT_REQUIRED)
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE IMGUI_DISABLE_SSE)

target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    glm::glm
    dv::processing
    Boost::iostreams
    Vulkan::Vulkan
    MetavisionSDK::core
    MetavisionSDK::stream
    Metavision::HAL
    Eigen3::Eigen
    ${PCL_LIBRARIES}
    yaml-cpp::yaml-cpp
    ${CERES_LIBRARIES}
    TBB::tbb
)
option(NOVA_BUILD_TESTS "Build NOVA test suite" ON)
if(NOVA_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
