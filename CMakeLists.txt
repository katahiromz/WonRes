# CMakeLists.txt --- CMake project settings
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.10)

# Enable testing
enable_testing()

# project name, version, and languages
project(WonRes VERSION 1.4 LANGUAGES C RC)

# Unicode support
add_definitions(-DUNICODE -D_UNICODE)

# libWonRes.a
add_library(WonRes STATIC loader.c string.c updater.c)
target_include_directories(WonRes INTERFACE .)
target_link_libraries(WonRes PRIVATE imagehlp)

# tests
add_subdirectory(tests)
