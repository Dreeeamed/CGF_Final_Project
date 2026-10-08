cmake_minimum_required(VERSION 3.16)
project(SimpleCastleMidterm LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(glad CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)

add_executable(simple_castle
    src/main.cpp
    src/castle_geometry.cpp
    src/material.cpp
)
target_include_directories(simple_castle PRIVATE src)
target_link_libraries(simple_castle PRIVATE glfw glad::glad glm::glm)

# Shader text files are read relative to the working directory.
set_property(TARGET simple_castle PROPERTY
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}")

