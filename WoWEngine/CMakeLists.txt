cmake_minimum_required(VERSION 3.20)

# The version, for the programs (Version.h) and the release (tools/make_release.py reads it from here).
# Change it only here.
project(WoWEngine VERSION 0.2.2 LANGUAGES C CXX)

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
    src/core/Palette.cpp
    src/core/Smacker.cpp
    src/core/Sprite.cpp
    src/core/TextOjd.cpp
    src/core/ObjOjd.cpp
    src/core/Research.cpp
    src/core/Font.cpp
    src/core/Vfs.cpp
    src/core/Bmp.cpp
    src/core/BuildList.cpp
    src/core/ShadeTable.cpp
    src/core/SaveGame.cpp
    src/core/SaveFile.cpp
    src/core/SoundTable.cpp
    src/core/Models.cpp
    src/core/Landscape.cpp
    src/core/TerrainView.cpp
    src/core/Scenery.cpp
    src/core/IsoObject.cpp
    src/core/Bridge.cpp
    src/core/Deployment.cpp
    src/core/WofView.cpp
    src/core/AiTable.cpp
)
target_include_directories(wowcore PUBLIC src ${CMAKE_CURRENT_BINARY_DIR}/generated)
configure_file(src/Version.h.in ${CMAKE_CURRENT_BINARY_DIR}/generated/Version.h @ONLY)

# --- installer: copies the game from the discs or their images and rips the music (no SDL) ---
add_library(wowinstall STATIC
    src/install/Disc.cpp
    src/install/DriveAudio.cpp
    src/install/Iso9660.cpp
    src/install/Installer.cpp
)
target_link_libraries(wowinstall PUBLIC wowcore)
# IniStore.h (shared with the patch DLLs): the installer writes the install's language into WoWRevived.ini
target_include_directories(wowinstall PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../WoWPatches/Common)
# The two command line tools are for working on the engine, not for players: they aren't built unless asked for
# (cmake -DWOW_BUILD_TOOLS=ON) and aren't in a release.
#   wowinstall-cli: WoWLauncher's install from the discs, as a command with its steps printed
#   wowdump:        lists and extracts the archives, prints the tables and saves, renders sprites and models
option(WOW_BUILD_TOOLS "Build the developer command line tools (wowdump, wowinstall-cli)" OFF)
if(WOW_BUILD_TOOLS)
    add_executable(wowinstall-cli tools/WoWInstall.cpp)
    target_link_libraries(wowinstall-cli PRIVATE wowinstall)
    add_executable(wowdump tools/wowdump.cpp)
    target_link_libraries(wowdump PRIVATE wowcore)
endif()

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
        src/platform/Music.cpp
        src/platform/Sound.cpp
        src/platform/Discord.cpp
        src/frontend/ResearchScreen.cpp
        src/frontend/Resources.cpp
        src/frontend/Canvas.cpp
        src/frontend/Settings.cpp
        src/frontend/Screen.cpp
        src/frontend/Menu.cpp
        src/frontend/CdPlayer.cpp
        src/frontend/Briefing.cpp
        src/frontend/Credits.cpp
        src/frontend/FrontEnd.cpp
        src/frontend/MovieScreen.cpp
        src/frontend/Splash.cpp
        src/platform/Movie.cpp
        src/battlemap/BattleMap.cpp
        src/battlemap/Driving.cpp
        src/battlemap/Routes.cpp
        src/battlemap/Combat.cpp
        src/battlemap/Weapons.cpp
    src/battlemap/Effects.cpp
    src/battlemap/Reinforcements.cpp
    src/battlemap/Sinking.cpp
    src/battlemap/BattleAi.cpp
    src/battlemap/Fog.cpp
        src/warmap/WarMap.cpp
        src/warmap/Production.cpp
        src/warmap/BuildWindow.cpp
        src/warmap/WarMapAi.cpp
    )
    if(WIN32)
        # WoWRevived's .mp4 movies through Media Foundation
        target_sources(WoWEngine PRIVATE src/platform/Mp4Movie.cpp)
        target_link_libraries(WoWEngine PRIVATE mfplat mfreadwrite mfuuid ole32)
    endif()
    # IniStore.h is shared with the patch DLLs so the engine reads the same WoWRevived.ini
    target_include_directories(WoWEngine PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../WoWPatches/Common)
    target_link_libraries(WoWEngine PRIVATE wowcore SDL3::SDL3)
    # a build run where it was made finds the engine's loose files (dat, maps) in the source folder
    target_compile_definitions(WoWEngine PRIVATE WOWENGINE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    if(WIN32)
        # the programs' icon (the game's own, as WoWLauncher's C# build has it)
        target_sources(WoWEngine PRIVATE resources/WoWEngine.rc)
    endif()

    # WoWLauncher: installs the game from the discs, then starts it as the Human or Martian side
    add_executable(WoWLauncher WIN32 MACOSX_BUNDLE
        src/launcher/main.cpp
        src/launcher/Ui.cpp
        src/platform/Video.cpp
    )
    target_link_libraries(WoWLauncher PRIVATE wowinstall SDL3::SDL3)
    if(WIN32)
        target_sources(WoWLauncher PRIVATE resources/WoWEngine.rc)
    endif()
    # a build run where it was made finds the updated text in the source's resources folder
    target_compile_definitions(WoWLauncher PRIVATE WOWENGINE_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    # the launcher's picture, next to the programs
    add_custom_command(TARGET WoWLauncher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/resources/launcher.bmp $<TARGET_FILE_DIR:WoWLauncher>/launcher.bmp)
    if(WIN32)
        # keep SDL3.dll next to the exe
        add_custom_command(TARGET WoWEngine POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:WoWEngine>)
        # WoWRevived's updated TEXT.ojd for each language, in "resources" beside the programs: WoWLauncher copies
        # the install's language's in as the game's TEXT.ojd (src/install/Installer.cpp)
        foreach(text_language EN FR DE IT ES)
            if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/TEXT-${text_language}.ojd)
                add_custom_command(TARGET WoWLauncher POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/resources/TEXT-${text_language}.ojd $<TARGET_FILE_DIR:WoWLauncher>/resources/TEXT-${text_language}.ojd)
            endif()
        endforeach()
        # Discord's rich presence (src/platform/Discord.cpp): discord-rpc.dll and its licence next to the exe. The
        # DLL has to be built for the same processor as the engine (x64 for the x64 presets), or it won't load.
        if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/resources/discord-rpc.dll)
            add_custom_command(TARGET WoWEngine POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/resources/discord-rpc.dll $<TARGET_FILE_DIR:WoWEngine>/discord-rpc.dll
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_CURRENT_SOURCE_DIR}/resources/discord-rpc-LICENSE.txt $<TARGET_FILE_DIR:WoWEngine>/discord-rpc-LICENSE.txt)
        endif()
    endif()
    if(NOT WIN32)
        # a release (tools/make_release.py) has the SDL3 library beside the programs, so they look for it in their own
        # folder ($ORIGIN, @loader_path on macOS); BUILD_RPATH_USE_ORIGIN keeps the build's own path to _deps relative,
        # so a build folder that is moved still works (update 234: the release found SDL3 only in the folder it was
        # built in)
        if(APPLE)
            set(wow_origin "@loader_path")
        else()
            set(wow_origin "\$ORIGIN")
        endif()
        set_target_properties(WoWEngine WoWLauncher PROPERTIES BUILD_RPATH_USE_ORIGIN ON BUILD_RPATH "${wow_origin}")
    endif()
else()
    message(WARNING "SDL3 not available: only the libraries (and the tools, with WOW_BUILD_TOOLS) will be built")
endif()
