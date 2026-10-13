cmake_minimum_required(VERSION 3.26.0 FATAL_ERROR)

set(CMAKE_SYSTEM_VERSION 10.0 CACHE STRING "" FORCE)
set(CMAKE_CXX_STANDARD 20 CACHE STRING "The C++ standard to use")
set(CMAKE_C_STANDARD 23 CACHE STRING "The C standard to use")
set(CMAKE_OSX_DEPLOYMENT_TARGET "10.15" CACHE STRING "Minimum OS X deployment version")

project(Metroid64 VERSION 0.1.0 LANGUAGES C CXX)

set(SPDLOG_LEVEL_TRACE 0)
set(SPDLOG_LEVEL_OFF 6)
set(SPDLOG_MIN_CUTOFF SPDLOG_LEVEL_TRACE CACHE STRING "cutoff at trace")

option(SUPPRESS_WARNINGS "Suppress warnings in libultraship" ON)
if(SUPPRESS_WARNINGS)
    if(MSVC)
        set(WARNING_OVERRIDE /w)
    else()
        set(WARNING_OVERRIDE -w)
    endif()
endif()

set_property(DIRECTORY ${CMAKE_SOURCE_DIR} PROPERTY VS_STARTUP_PROJECT metroid64)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/MP>)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/utf-8>)
add_compile_options($<$<CXX_COMPILER_ID:MSVC>:/Zc:preprocessor>)

if(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    if(NOT CMAKE_VS_PLATFORM_NAME)
        set(CMAKE_VS_PLATFORM_NAME "x64")
    endif()
    include(CMake/automate-vcpkg.cmake)
    set(VCPKG_TRIPLET x64-windows-static)
    set(VCPKG_TARGET_TRIPLET x64-windows-static)
    vcpkg_bootstrap()
    vcpkg_install_packages(zlib bzip2 libzip libpng sdl2 sdl2-net glew glfw3 nlohmann-json tinyxml2 spdlog libogg libvorbis opus opusfile)
endif()

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Choose the type of build." FORCE)
endif()

include(CMake/Utils.cmake)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/CMake")

################################################################################
# libultraship (Shipwright's engine: window, Fast3D renderer, input, ImGui, archives)
################################################################################
set(GBI_UCODE F3DEX_GBI_2)
add_compile_definitions(CONTROLLERBUTTONS_T=uint32_t)

# spdlog: prefer the system package, fall back to fetching it (clang >= 21 needs fmt >= 11.1).
include(FetchContent)
set(_use_system_spdlog ON)
if(CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND CMAKE_CXX_COMPILER_VERSION VERSION_GREATER_EQUAL "21.0")
    find_package(fmt QUIET)
    if(fmt_FOUND AND fmt_VERSION VERSION_LESS "11.1")
        set(_use_system_spdlog OFF)
    endif()
endif()
if(_use_system_spdlog)
    find_package(spdlog 1.10 QUIET)
endif()
if(NOT spdlog_FOUND)
    FetchContent_Declare(
        spdlog
        GIT_REPOSITORY https://github.com/gabime/spdlog.git
        GIT_TAG v1.16.0
        OVERRIDE_FIND_PACKAGE
    )
    FetchContent_MakeAvailable(spdlog)
endif()

set(LUS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/libultraship)
add_subdirectory(libultraship ${CMAKE_BINARY_DIR}/libultraship)
target_compile_options(libultraship PRIVATE "${WARNING_OVERRIDE}")

# libultraship downloads stb_image.h at configure time and silently leaves an empty file when
# that fails (offline/proxied builds); fall back to a system copy (e.g. Debian's libstb-dev).
set(_stb_header ${CMAKE_BINARY_DIR}/_deps/stb/stb_image.h)
file(SIZE ${_stb_header} _stb_size)
if(_stb_size EQUAL 0)
    find_file(SYSTEM_STB_IMAGE stb_image.h PATHS /usr/include/stb /usr/local/include/stb NO_DEFAULT_PATH)
    if(SYSTEM_STB_IMAGE)
        message(STATUS "stb_image.h download failed, using ${SYSTEM_STB_IMAGE}")
        configure_file(${SYSTEM_STB_IMAGE} ${_stb_header} COPYONLY)
    else()
        message(FATAL_ERROR "Could not download stb_image.h; install libstb-dev or check your network")
    endif()
endif()

################################################################################
# Metroid64
################################################################################
add_subdirectory(metroid64)

if(CMAKE_SYSTEM_NAME MATCHES "Windows")
    set(CPACK_GENERATOR "ZIP")
elseif(CMAKE_SYSTEM_NAME MATCHES "Linux")
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
