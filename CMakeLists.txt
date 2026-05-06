# CMakeLists.txt --- CMake project settings
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.10)

# project name and language
project(WonRes C)

# Unicode support
add_definitions(-DUNICODE -D_UNICODE)

# libWonRes.a
add_library(WonRes STATIC loader.c updater.c)
target_link_libraries(WonRes PRIVATE imagehlp)

# test-Types.exe
add_executable(test-Types test-Types.c)
target_link_libraries(test-Types PRIVATE WonRes)

# test-Names.exe
add_executable(test-Names test-Names.c)
target_link_libraries(test-Names PRIVATE WonRes)

# test-Langs.exe
add_executable(test-Langs test-Langs.c)
target_link_libraries(test-Langs PRIVATE WonRes)
