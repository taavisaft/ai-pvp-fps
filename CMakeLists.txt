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
    src/projectiles.cpp
    src/tree_collision.cpp
    src/forest_site.cpp
    src/net_common.cpp
    src/tree_scatter.cpp)

set(CLIENT_SOURCES
    src/main.cpp
    src/player_visual.cpp
    src/gl_loader.cpp
    src/shader.cpp
    src/font.cpp
    src/mesh.cpp
    src/texture.cpp
    src/material.cpp
    src/terrain_render.cpp
    src/vegetation.cpp
    src/veg_mesh.cpp
    src/renderer.cpp
    src/hud.cpp
    src/perf.cpp
    src/camera.cpp
    src/input.cpp
    src/network.cpp
    src/lobby.cpp
    src/audio.cpp)

add_executable(game ${CLIENT_SOURCES} ${SHARED_SOURCES})
target_include_directories(game PRIVATE src)
target_link_libraries(game PRIVATE SDL2::SDL2 OpenGL::GL glm::glm)

add_executable(server src/server_main.cpp src/server_fire.cpp src/server_rewind.cpp ${SHARED_SOURCES})
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

# Copy sound assets next to the binary (optional; synth fallback if absent)
add_custom_command(TARGET game POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/sounds $<TARGET_FILE_DIR:game>/sounds)


include(CTest)
if(BUILD_TESTING)
    add_executable(server_fire_tests tests/server_fire_tests.cpp src/server_fire.cpp src/server_rewind.cpp
        src/physics.cpp src/projectiles.cpp src/tree_collision.cpp src/tree_scatter.cpp src/forest_site.cpp)
    target_include_directories(server_fire_tests PRIVATE src)
    target_link_libraries(server_fire_tests PRIVATE glm::glm)
    add_test(NAME server_fire COMMAND server_fire_tests)
    add_executable(client_fire_tests tests/client_fire_tests.cpp src/network.cpp src/net_common.cpp)
    target_include_directories(client_fire_tests PRIVATE src)
    target_link_libraries(client_fire_tests PRIVATE glm::glm)
    if(WIN32)
        target_link_libraries(client_fire_tests PRIVATE ws2_32)
    endif()
    add_test(NAME client_fire COMMAND client_fire_tests)
    add_executable(tree_collision_tests tests/tree_collision_tests.cpp
        src/physics.cpp src/projectiles.cpp src/tree_collision.cpp src/tree_scatter.cpp src/forest_site.cpp)
    target_include_directories(tree_collision_tests PRIVATE src)
    target_link_libraries(tree_collision_tests PRIVATE glm::glm)
    add_test(NAME tree_collision COMMAND tree_collision_tests)
    add_executable(forest_site_tests tests/forest_site_tests.cpp
        src/physics.cpp src/tree_scatter.cpp src/forest_site.cpp)
    target_include_directories(forest_site_tests PRIVATE src)
    target_link_libraries(forest_site_tests PRIVATE glm::glm)
    add_test(NAME forest_site COMMAND forest_site_tests)
    find_package(Python3 COMPONENTS Interpreter QUIET)
    if(Python3_Interpreter_FOUND)
        add_test(NAME server_fire_udp COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/tests/server_fire_udp.py $<TARGET_FILE:server>)
        set_tests_properties(server_fire_udp PROPERTIES TIMEOUT 20)
    endif()
endif()
