# CMakeLists.txt --- CMake project settings
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.10)

# Enable testing
enable_testing()

# project name, version, and languages
project(WonRes VERSION 1.5 LANGUAGES C RC)

# Unicode support
add_definitions(-DUNICODE -D_UNICODE)

# statically link
if (WIN32)
    if (CMAKE_CXX_COMPILER_ID STREQUAL "Clang")
        # using Clang
        set(CMAKE_C_FLAGS "-static -W -Wno-missing-field-initializers")
        set(CMAKE_CXX_FLAGS "-static -W -Wno-missing-field-initializers")
    elseif (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # using GCC
        set(CMAKE_C_FLAGS "-static -W -Wno-missing-field-initializers")
        set(CMAKE_CXX_FLAGS "-static -W -Wno-missing-field-initializers")
    elseif (MSVC)
        # replace "/MD" with "/MT" (building without runtime DLLs)
        set(CompilerFlags
            CMAKE_C_FLAGS
            CMAKE_C_FLAGS_DEBUG
            CMAKE_C_FLAGS_RELEASE
            CMAKE_C_FLAGS_MINSIZEREL
            CMAKE_C_FLAGS_RELWITHDEBINFO
            CMAKE_CXX_FLAGS
            CMAKE_CXX_FLAGS_DEBUG
            CMAKE_CXX_FLAGS_RELEASE
            CMAKE_CXX_FLAGS_MINSIZEREL
            CMAKE_CXX_FLAGS_RELWITHDEBINFO)
        foreach(CompilerFlags ${CompilerFlags})
            string(REPLACE "/MD" "/MT" ${CompilerFlags} "${${CompilerFlags}}")
        endforeach()
    endif()
endif()

# libWonRes.a
add_library(WonRes STATIC loader.c string.c updater.c)
target_include_directories(WonRes INTERFACE .)
target_link_libraries(WonRes PRIVATE imagehlp)

# tests
add_subdirectory(tests)
