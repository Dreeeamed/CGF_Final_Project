cmake_minimum_required(VERSION 3.24)
project(CGF_Final_Project VERSION 0.1.0 LANGUAGES C CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

include(FetchContent)

# All three teammates build the same dependency versions.
set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(GLFW_LIBRARY_TYPE STATIC CACHE STRING "" FORCE)
FetchContent_Declare(glfw
    URL https://github.com/glfw/glfw/archive/refs/tags/3.4.zip
    URL_HASH SHA256=a133ddc3d3c66143eba9035621db8e0bcf34dba1ee9514a9e23e96afd39fd57a
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

set(GLM_BUILD_LIBRARY OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLM_BUILD_INSTALL OFF CACHE BOOL "" FORCE)
FetchContent_Declare(glm
    URL https://github.com/g-truc/glm/archive/refs/tags/1.0.1.zip
    URL_HASH SHA256=09c5716296787e1f7fcb87b1cbdbf26814ec1288ed6259ccd30d5d9795809fa5
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(glfw glm)

# Checked-in generated loader: no Python or GLAD installation is needed.
add_library(glad STATIC third_party/glad/src/glad.c)
add_library(glad::glad ALIAS glad)
target_include_directories(glad PUBLIC third_party/glad/include)
target_link_libraries(glad PRIVATE ${CMAKE_DL_LIBS})

add_executable(simple_castle
    src/main.cpp
    src/castle_geometry.cpp
    src/hall_geometry.cpp
    src/material.cpp
)
target_include_directories(simple_castle PRIVATE src third_party/stb)
target_link_libraries(simple_castle PRIVATE glfw glad::glad glm::glm)
target_compile_definitions(simple_castle PRIVATE GLFW_INCLUDE_NONE)

# Keep runtime files beside the executable, including after shader-only edits.
add_custom_target(castle_resources ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/shaders" "$<TARGET_FILE_DIR:simple_castle>/shaders"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:simple_castle>/assets"
    VERBATIM
)
add_dependencies(simple_castle castle_resources)

# The root also contains shaders, so the IDE may use it as its working directory.
set_property(TARGET simple_castle PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT simple_castle)

