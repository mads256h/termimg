cmake_minimum_required(VERSION 3.21)
project(termimg)

set(CMAKE_CXX_STANDARD 20)

add_executable(termimg main.cpp)
target_link_libraries(termimg PRIVATE project_warnings X11 XRes Imlib2 procps)

# Link this 'library' to use the warnings specified in CompilerWarnings.cmake
add_library(project_warnings INTERFACE)

# standard compiler warnings
include(cmake/CompilerWarnings.cmake)
set_project_warnings(project_warnings)
