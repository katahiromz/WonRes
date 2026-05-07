# CMakeLists.txt --- CMake project settings
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.10)

# Enable testing
enable_testing()

# project name and language
project(WonRes C)

# Unicode support
add_definitions(-DUNICODE -D_UNICODE)

# libWonRes.a
add_library(WonRes STATIC loader.c updater.c)
target_include_directories(WonRes INTERFACE .)
target_link_libraries(WonRes PRIVATE imagehlp)

# tests
add_subdirectory(tests)
