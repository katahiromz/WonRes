# CMakeLists.txt --- CMake project settings
##############################################################################

# CMake minimum version
cmake_minimum_required(VERSION 3.10)

# Enable testing
enable_testing()

# project name, version, and languages
project(WonRes VERSION 2.1 LANGUAGES C RC)

# Unicode support
add_definitions(-DUNICODE -D_UNICODE)

# option WONRES_ENABLE_CRYPTO
option(WONRES_ENABLE_CRYPTO "Enable WonRes encryption" ON)
if(WONRES_ENABLE_CRYPTO)
    add_definitions(-DWONRES_ENABLE_CRYPTO=1)
endif()
message(STATUS "WONRES_ENABLE_CRYPTO: ${WONRES_ENABLE_CRYPTO}")

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
add_library(WonRes STATIC
    accel.c
    anicursor.c
    bitmap.c
    crypto.c
    dialog.c
    formatmsg.c
    iconcursor.c
    loader.c
    loadimage.c
    menu.c
    string.c
    updater.c)
target_include_directories(WonRes INTERFACE .)
target_link_libraries(WonRes PRIVATE imagehlp advapi32 gdi32 user32)

# tests
add_subdirectory(tests)
