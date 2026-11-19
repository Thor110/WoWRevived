cmake_minimum_required(VERSION 3.20)
project(WoWEngine VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(WOW_FETCH_SDL "Download and build SDL3 when it isn't installed" ON)
set(WOW_SDL_TAG "release-3.2.30" CACHE STRING "SDL3 release used when downloading")

if(MSVC)
    add_compile_options(/W4 /utf-8 /permissive-)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# --- core: file formats and game data, no SDL so the tools and the Sieve can use it on their own ---
add_library(wowcore STATIC
    src/core/Archive.cpp
    src/core/Ffuh.cpp
    src/core/GameFiles.cpp
)
target_include_directories(wowcore PUBLIC src)

# --- wowdump: command line archive lister / extractor ---
add_executable(wowdump tools/wowdump.cpp)
target_link_libraries(wowdump PRIVATE wowcore)

# --- the engine itself ---
find_package(SDL3 CONFIG QUIET)
if(NOT SDL3_FOUND AND WOW_FETCH_SDL)
    include(FetchContent)
    message(STATUS "SDL3 not found, downloading ${WOW_SDL_TAG}")
    set(SDL_SHARED ON CACHE BOOL "" FORCE)
    set(SDL_STATIC OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG ${WOW_SDL_TAG}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)
endif()

if(TARGET SDL3::SDL3)
    add_executable(WoWEngine WIN32 MACOSX_BUNDLE
        src/main.cpp
        src/platform/Video.cpp
    )
    target_link_libraries(WoWEngine PRIVATE wowcore SDL3::SDL3)
    if(WIN32)
        # keep SDL3.dll next to the exe
        add_custom_command(TARGET WoWEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:WoWEngine>)
    endif()
else()
    message(WARNING "SDL3 not available: only wowcore and wowdump will be built")
endif()
