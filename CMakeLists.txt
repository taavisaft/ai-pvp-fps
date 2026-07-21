cmake_minimum_required(VERSION 3.20)
project(pvp_shooter CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL2 REQUIRED)
find_package(OpenGL REQUIRED)

include(FetchContent)
# glm 0.9.9.8 declares a pre-3.5 cmake_minimum_required, rejected by CMake 4+
set(CMAKE_POLICY_VERSION_MINIMUM 3.5)
FetchContent_Declare(glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG        0.9.9.8)
FetchContent_MakeAvailable(glm)

set(SHARED_SOURCES
    src/physics.cpp
    src/net_common.cpp)

set(CLIENT_SOURCES
    src/main.cpp
    src/gl_loader.cpp
    src/shader.cpp
    src/font.cpp
    src/mesh.cpp
    src/texture.cpp
    src/material.cpp
    src/foliage.cpp
    src/building.cpp
    src/props.cpp
    src/grass.cpp
    src/renderer.cpp
    src/hud.cpp
    src/camera.cpp
    src/input.cpp
    src/network.cpp
    src/lobby.cpp
    src/audio.cpp)

add_executable(game ${CLIENT_SOURCES} ${SHARED_SOURCES})
target_include_directories(game PRIVATE src)
target_link_libraries(game PRIVATE SDL2::SDL2 OpenGL::GL glm::glm)

add_executable(server src/server_main.cpp ${SHARED_SOURCES})
target_include_directories(server PRIVATE src)
target_link_libraries(server PRIVATE glm::glm)

if(WIN32)
    target_link_libraries(game PRIVATE ws2_32)
    target_link_libraries(server PRIVATE ws2_32)
    target_compile_definitions(game PRIVATE _WIN32_WINNT=0x0601)
    target_compile_definitions(server PRIVATE _WIN32_WINNT=0x0601)
endif()

# Copy shaders next to the binary. A standalone ALL target (not POST_BUILD) so a
# shader-only edit still refreshes the copy — POST_BUILD skips when game doesn't relink.
add_custom_target(copy_shaders ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:game>/shaders)
add_dependencies(copy_shaders game)

# Copy image textures next to the binary (optional; procedural fallback if absent)
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/textures $<TARGET_FILE_DIR:game>/textures)

# Copy 3D models (gun, trees) next to the binary
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/models $<TARGET_FILE_DIR:game>/models)

# Copy sound assets next to the binary (optional; synth fallback if absent)
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/sounds $<TARGET_FILE_DIR:game>/sounds)

# Selected source resources used by the optimized forest biomes. Copy only the small
# runtime textures, not the multi-format source model archive.
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:game>/resources/terrain
    COMMAND ${CMAKE_COMMAND} -E make_directory
        $<TARGET_FILE_DIR:game>/resources/grass
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/_RESOURCES/terrain/textures_forest_ground_1/forrest_ground_01_diff_1k.jpg
        $<TARGET_FILE_DIR:game>/resources/terrain/forest_ground.jpg
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/_RESOURCES/grass/grass_medium_01_1k/textures/grass_medium_01_diff_1k.jpg
        $<TARGET_FILE_DIR:game>/resources/grass/grass_medium_01_diff.jpg
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        ${CMAKE_SOURCE_DIR}/_RESOURCES/grass/grass_medium_01_1k/textures/grass_medium_01_alpha_1k.png
        $<TARGET_FILE_DIR:game>/resources/grass/grass_medium_01_alpha.png
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/_RESOURCES/trees/Random
        $<TARGET_FILE_DIR:game>/resources/trees)
