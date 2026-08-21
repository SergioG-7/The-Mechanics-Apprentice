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
    src/Entities/Cylinder.cpp
    src/Entities/Hazard.cpp
    src/Entities/Gear.cpp
    src/Entities/Door.cpp
    src/Entities/EnemyFactory.cpp
    src/Entities/Spawner.cpp
    src/VFX/ParticleSystem.cpp
    src/Core/EndlessDirector.cpp
    src/UI/MenuScreen.cpp
    src/UI/HudRenderer.cpp
    src/Entities/HealthKit.cpp
    src/Entities/ExplosiveBarrel.cpp
    src/Entities/PowerUp.cpp
    src/Entities/ElectricTile.cpp
    src/Renderer/CameraRig.cpp
    src/Core/DropTable.cpp
    src/Core/SaveManager.cpp
    src/Core/LocalizationManager.cpp
    src/Core/AudioSettings.cpp
    src/Audio/MusicController.cpp
)

target_include_directories(${PROJECT_NAME} PRIVATE src)
target_link_libraries(${PROJECT_NAME} PRIVATE raylib nlohmann_json::nlohmann_json)
target_compile_options(${PROJECT_NAME} PRIVATE /W4)

# --- Build de Release: máxima optimización y sin ventana de consola ---
# Solo en Release ($<CONFIG:Release>) -- en Debug se quiere la consola
# abierta para ver TraceLog/std::cout mientras se desarrolla. main.cpp
# sigue siendo un int main() normal: /ENTRY:mainCRTStartup redirige el
# punto de entrada al arranque de la CRT compatible con main() aunque el
# subsistema sea WINDOWS (que por defecto esperaría WinMain), así que no
# hace falta reescribir main() para ocultar la consola.
if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/O2>)
    target_link_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS>
        $<$<CONFIG:Release>:/ENTRY:mainCRTStartup>
    )
else()
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:-O3>)
    target_link_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:-mwindows>)
endif()

# Copia shaders/assets junto al ejecutable en cada build. VS2022 depura con
# el "working directory" apuntando por defecto a la carpeta del propio .exe,
# así que las rutas relativas ("shaders/toon.vs") se resuelven solas al darle a Play.
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/shaders $<TARGET_FILE_DIR:${PROJECT_NAME}>/shaders
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_SOURCE_DIR}/assets $<TARGET_FILE_DIR:${PROJECT_NAME}>/assets
)
