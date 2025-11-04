cmake_minimum_required(VERSION 3.22...3.28)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(NOVA LANGUAGES CXX C)

file(GLOB_RECURSE SOURCES "src/*.cc")
file(GLOB_RECURSE HEADERS "src/*.hh")

find_package(sdl3 REQUIRED)
find_package(sdl3_ttf REQUIRED)
find_package(glm REQUIRED)
find_package(Eigen3 CONFIG REQUIRED)
find_package(dv-processing CONFIG REQUIRED)

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

add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS})

target_include_directories(${PROJECT_NAME} PRIVATE ${IMGUI_DIR})
target_include_directories(${PROJECT_NAME} PRIVATE "resources/")
target_link_libraries(${PROJECT_NAME} PRIVATE
    SDL3::SDL3
    SDL3_ttf::SDL3_ttf
    glm::glm
    Eigen3::Eigen
    dv::processing
)