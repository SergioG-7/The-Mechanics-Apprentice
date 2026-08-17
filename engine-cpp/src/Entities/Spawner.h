#pragma once
#include "raylib.h"
#include "Entity.h"
#include "Enemy.h"
#include <memory>
#include <string>
#include <vector>

// Genera enemigos de un arquetipo fijo (ver EnemyFactory) a intervalos
// regulares, hasta un tope de vivos simultáneos "propios". No es un Entity:
// no ocupa espacio ni se dibuja, solo lo tickea Application.
class Spawner {
public:
    Spawner(Vector3 position, std::string enemyType, float spawnInterval, int maxEnemies,
            const std::vector<std::unique_ptr<Entity>>* obstacles, Shader shader);

    // Empuja un Enemy nuevo a activeEnemies cuando toca -- es la misma lista
    // que dibuja/actualiza Application (m_level.enemies), así el recién
    // nacido entra en el resto del juego sin ningún camino aparte.
    void Update(float dt, std::vector<std::unique_ptr<Enemy>>& activeEnemies);

    // Reduce el intervalo multiplicándolo por factor (< 1 lo acorta), con un
    // suelo mínimo para no degenerar en spawns cada frame. La usa
    // EndlessDirector para escalar la dificultad del Modo Infinito.
    void ScaleSpawnInterval(float factor);

private:
    Vector3 m_position;
    std::string m_enemyType;
    float m_spawnInterval;
    int m_maxEnemies;
    float m_timer;

    // Enemigos vivos generados por ESTE spawner, por puntero no propietario
    // (la propiedad real vive en activeEnemies) -- solo para poder contar
    // cuántos de los suyos siguen vivos y respetar m_maxEnemies.
    std::vector<Enemy*> m_spawnedEnemies;

    const std::vector<std::unique_ptr<Entity>>* m_obstacles;
    Shader m_shader;

    static constexpr float kMinSpawnInterval = 0.5f;
};
