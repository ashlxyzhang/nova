cmake_minimum_required(VERSION 3.22...3.28)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(NOVA LANGUAGES C CXX)

file(GLOB_RECURSE SOURCES "src/*.cc")
file(GLOB_RECURSE HEADERS "src/*.hh")

# Use FetchContent on Linux, and find_package (vcpkg) on Windows/Other
include(FetchContent)

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    FetchContent_Declare(
        SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG release-3.2.26
    )
    FetchContent_MakeAvailable(SDL3)

    FetchContent_Declare(
        SDL3_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG release-3.2.2
    )
    FetchContent_MakeAvailable(SDL3_ttf)

    FetchContent_Declare(
        glm
        GIT_REPOSITORY https://github.com/g-truc/glm.git
        GIT_TAG 1.0.2
    )
    FetchContent_MakeAvailable(glm)

    FetchContent_Declare(
        eigen
        GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
        GIT_TAG 3.4.0
    )
    FetchContent_MakeAvailable(eigen)

    FetchContent_Declare(
        dv-processing
        GIT_REPOSITORY https://gitlab.com/inivation/dv/dv-processing.git
        GIT_TAG 2.0.2
    )
    FetchContent_MakeAvailable(dv-processing)

else()
    find_package(SDL3 REQUIRED)
    find_package(SDL3_ttf REQUIRED)
    find_package(glm REQUIRED)
    find_package(Eigen3 REQUIRED)
    find_package(dv-processing REQUIRED)
    find_package(Boost COMPONENTS iostreams REQUIRED)

endif()

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
    Boost::iostreams
)
