#pragma once
#include "raylib.h"
#include "Entity.h"
#include "Enemy.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Un arquetipo de enemigo y su peso relativo dentro de un spawner aleatorio.
struct WeightedEnemyType {
    std::string name;
    int weight = 1;
};

// Genera enemigos a intervalos regulares, hasta un tope de vivos a la vez.
// Puede ser de un arquetipo fijo o sortear uno por peso en cada spawn.
class Spawner {
public:
    Spawner(Vector3 position, std::string enemyType, float spawnInterval, int maxEnemies,
            const std::vector<std::unique_ptr<Entity>>* obstacles, Shader shader,
            std::vector<WeightedEnemyType> weightedTypes = {});

    // Genera un enemigo nuevo cuando toca y lo añade a la lista de enemigos activos.
    void Update(float dt, std::vector<std::unique_ptr<Enemy>>& activeEnemies);

    // Dibuja el marcador del spawner en el suelo.
    void Draw() const;

    Vector3 GetPosition() const { return m_position; }

    // Acelera o ralentiza el ritmo de generación multiplicando el intervalo.
    void ScaleSpawnInterval(float factor);

    // Deja de contar los enemigos propios que ya han sido destruidos.
    void ForgetDestroyedEnemies();

    // true si este spawner sortea el arquetipo entre varios en cada spawn.
    bool IsRandom() const { return !m_weightedTypes.empty(); }

private:
    // Elige el arquetipo del próximo enemigo a generar.
    std::string PickEnemyType() const;

    Vector3 m_position;
    std::string m_enemyType;
    std::vector<WeightedEnemyType> m_weightedTypes;
    float m_spawnInterval;
    int m_maxEnemies;
    float m_timer;

    // Enemigos vivos generados por este spawner, para contar cuántos hay y respetar el tope.
    std::vector<Enemy*> m_spawnedEnemies;

    const std::vector<std::unique_ptr<Entity>>* m_obstacles;
    Shader m_shader;

    static constexpr float kMinSpawnInterval = 0.5f;

    // Tope global de enemigos Buffer vivos a la vez, entre todos los spawners.
    static constexpr int kMaxLiveBuffers = 2;
};
