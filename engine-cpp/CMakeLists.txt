cmake_minimum_required(VERSION 3.16)
project(Level5Portfolio CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- raylib vía FetchContent: se descarga y compila sola, sin instalar nada a mano ---
include(FetchContent)
set(FETCHCONTENT_QUIET FALSE)
set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BUILD_GAMES    OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    raylib
    GIT_REPOSITORY https://github.com/raysan5/raylib.git
    GIT_TAG        6.0
    GIT_PROGRESS   TRUE
)
FetchContent_MakeAvailable(raylib)

# --- nlohmann/json vía FetchContent (confirmado para el LevelLoader del Paso 2) ---
FetchContent_Declare(
    json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.12.0
)
FetchContent_MakeAvailable(json)

add_executable(${PROJECT_NAME}
    src/main.cpp
    src/Core/Application.cpp
    src/Renderer/ShaderManager.cpp
    src/Renderer/ModelUtils.cpp
    src/Combat/CollisionMath.cpp
    src/IO/LevelLoader.cpp
    src/Combat/CombatSystem.cpp
    src/Entities/Entity.cpp
    src/Entities/Actor.cpp
    src/Entities/Player.cpp
    src/Entities/Enemy.cpp
    src/Entities/Obstacle.cpp
    src/Entities/Gear.cpp
    src/Entities/Door.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib nlohmann_json::nlohmann_json)
target_compile_options(${PROJECT_NAME} PRIVATE /W4)

# Copia shaders/assets junto al ejecutable en cada build. VS2022 depura con
# el "working directory" apuntando por defecto a la carpeta del propio .exe,
# así que las rutas relativas ("shaders/toon.vs") se resuelven solas al darle a Play.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)
