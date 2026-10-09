# =============================================================================
#  Breakout - Minimal Isometric Shooter Engine
#  Phase 1 : Engine Foundation & Isometric Math
#  Phase 2 : Procedural Generation & the 10-Level Manager
#  Phase 3 : Player Control, Combat & 2D Dynamic Lighting
#  Phase 4 : Enemy AI & Difficulty Scaling
#  Phase 5 : Game Loop Completion & Polish
#            (win/loss, level transition, lerped camera, particles, HUD)
# -----------------------------------------------------------------------------
#  Targets : macOS (Apple Silicon / arm64) + Windows (x64)
#  Stack   : C++17 + Raylib 5.0 (fetched at configure time) + CMake >= 3.16
#  Layout  : src/    -> all .cpp / .hpp sources
#            assets/ -> shaders + optional sprites (copied next to the binary)
#            build/  -> CMake output (git-ignored)
# =============================================================================

cmake_minimum_required(VERSION 3.16)

# -----------------------------------------------------------------------------
#  Apple Silicon detection.
#  This MUST run before project(): setting CMAKE_OSX_ARCHITECTURES early makes
#  the "-arch arm64" flag propagate to every target, including the Raylib
#  dependency that FetchContent downloads afterwards.
# -----------------------------------------------------------------------------
if(APPLE AND NOT DEFINED CMAKE_OSX_ARCHITECTURES)
    execute_process(
        COMMAND uname -m
        OUTPUT_VARIABLE BREAKOUT_HOST_ARCH
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )
    if(BREAKOUT_HOST_ARCH STREQUAL "arm64")
        set(CMAKE_OSX_ARCHITECTURES "arm64" CACHE STRING "macOS target architecture" FORCE)
        message(STATUS "[Breakout] Apple Silicon detected -> targeting arm64")
    endif()
endif()

project(Breakout VERSION 0.3.0 LANGUAGES CXX)

include(CTest)

# -----------------------------------------------------------------------------
#  Global C++ settings
# -----------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Raylib is compiled from source, so guarantee a sane single-config default.
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "Build type" FORCE)
endif()

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# -----------------------------------------------------------------------------
#  Raylib 5.0 via FetchContent (no pre-installed SDK required)
# -----------------------------------------------------------------------------
include(FetchContent)

# CMake 4.x dropped compatibility with `cmake_minimum_required(VERSION <3.5)`,
# while Raylib 5.0 still declares 3.0. Relax the floor for the dependency only;
# CMake <= 3.28 simply ignores this variable.
if(NOT DEFINED CMAKE_POLICY_VERSION_MINIMUM)
    set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
endif()

# Keep the downloaded Raylib build minimal - we only want the static library.
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)
set(PLATFORM       Desktop CACHE STRING "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        5.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(raylib)

# -----------------------------------------------------------------------------
#  Game executable
#  Phase 1 : main.cpp, IsometricMath.cpp, Renderer.cpp
#  Phase 2 : + MapGenerator.cpp, MapRenderer.cpp
#  Phase 3 : + PlayerSystem.cpp, CombatSystem.cpp, Props.cpp, LightingSystem.cpp
#  Phase 4 : + EnemySystem.cpp
#  Phase 5 : + ParticleSystem.cpp
# -----------------------------------------------------------------------------
add_executable(Breakout
    src/main.cpp
    src/CameraSystem.cpp
    src/SceneRenderer3D.cpp
    src/ProceduralScene3D.cpp
    src/MapMeshBuilder.cpp
    src/IsometricMath.cpp
    src/Renderer.cpp
    src/MapGenerator.cpp
    src/MapRenderer.cpp
    src/PlayerSystem.cpp
    src/CombatSystem.cpp
    src/Props.cpp
    src/LightingSystem.cpp
    src/EnemySystem.cpp
    src/ParticleSystem.cpp
)

target_include_directories(Breakout PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(Breakout PRIVATE raylib)

# -----------------------------------------------------------------------------
#  Runtime assets (Phase 3)
#  assets/shaders/lighting.fs and assets/sprites/ must be reachable at run time.
#  ResolveAssetPath() (World.hpp) probes three locations, in this order:
#    1. <current working directory>/assets/...   -> running from the source tree
#    2. <executable directory>/assets/...        -> running the built binary
#    3. BREAKOUT_SOURCE_ASSET_DIR/...            -> IDE launch from anywhere
#  The POST_BUILD copy below keeps (2) populated; the compile definition keeps
#  (3) valid no matter where the process is started from.
# -----------------------------------------------------------------------------
target_compile_definitions(Breakout PRIVATE
    BREAKOUT_SOURCE_ASSET_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets"
)

add_custom_command(TARGET Breakout POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:Breakout>/assets"
    COMMENT "[Breakout] copying assets/ next to the executable"
)

# Asset edits should refresh the runnable tree even when sources do not change.
file(GLOB_RECURSE BREAKOUT_RUNTIME_ASSETS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/assets/*"
)
set_property(TARGET Breakout APPEND PROPERTY LINK_DEPENDS ${BREAKOUT_RUNTIME_ASSETS})
add_custom_target(BreakoutAssets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/assets"
            "$<TARGET_FILE_DIR:Breakout>/assets"
    DEPENDS Breakout
    COMMENT "[Breakout] synchronizing runtime assets"
)

if(BUILD_TESTING)
    add_executable(BreakoutMathTests
        tests/PortMathTests.cpp
        src/CameraSystem.cpp
        src/IsometricMath.cpp
    )
    target_include_directories(BreakoutMathTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(BreakoutMathTests PRIVATE raylib)
    add_test(NAME breakout_math COMMAND BreakoutMathTests)

    add_executable(BreakoutGameplayTests
        tests/GameplayParityTests.cpp
        src/MapGenerator.cpp
    )
    target_include_directories(BreakoutGameplayTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(BreakoutGameplayTests PRIVATE raylib)
    add_test(NAME breakout_gameplay COMMAND BreakoutGameplayTests)

    add_executable(BreakoutMapMeshTests
        tests/MapMeshTests.cpp
        src/MapMeshBuilder.cpp
    )
    target_include_directories(BreakoutMapMeshTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(BreakoutMapMeshTests PRIVATE raylib)
    add_test(NAME breakout_map_mesh COMMAND BreakoutMapMeshTests)

    add_executable(BreakoutActorPoseTests
        tests/ActorPoseTests.cpp
    )
    target_include_directories(BreakoutActorPoseTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(BreakoutActorPoseTests PRIVATE raylib)
    add_test(NAME breakout_actor_pose COMMAND BreakoutActorPoseTests)

    add_executable(BreakoutCollision3DTests
        tests/Collision3DTests.cpp
    )
    target_include_directories(BreakoutCollision3DTests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src")
    target_link_libraries(BreakoutCollision3DTests PRIVATE raylib)
    add_test(NAME breakout_collision_3d COMMAND BreakoutCollision3DTests)
endif()

# -----------------------------------------------------------------------------
#  Platform specific flags
# -----------------------------------------------------------------------------
if(APPLE)
    # Apple Silicon: make the arm64 slice explicit for our own target as well.
    if(CMAKE_OSX_ARCHITECTURES MATCHES "arm64")
        target_compile_options(Breakout PRIVATE -arch arm64)
        target_link_options(Breakout PRIVATE -arch arm64)
    endif()

    target_compile_options(Breakout PRIVATE -Wall -Wextra)
elseif(WIN32)
    # Windows x64: Raylib pulls in the Win32 API, so trim the windows headers
    # and silence the MSVC-specific annoyances.
    target_compile_definitions(Breakout PRIVATE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    if(MSVC)
        # /utf-8 keeps the UTF-8 sources and the console output consistent.
        target_compile_options(Breakout PRIVATE /W4 /permissive- /utf-8)
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        message(STATUS "[Breakout] Windows x64 toolchain detected")
    else()
        message(WARNING "[Breakout] A 32-bit toolchain was detected - configure with -A x64 for the supported target")
    endif()
endif()
