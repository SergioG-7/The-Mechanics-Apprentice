#pragma once
#include "raylib.h"
#include "Entity.h"
#include "Enemy.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

// Un arquetipo y su peso relativo dentro de un spawner aleatorio. Los pesos
// no tienen que sumar nada en concreto: se normalizan contra su propio total
// (ver Spawner::PickEnemyType), así que {Runner:5, Tank:1} y {Runner:50,
// Tank:10} son lo mismo.
struct WeightedEnemyType {
    std::string name;
    int weight = 1;
};

// Genera enemigos a intervalos regulares, hasta un tope de vivos simultáneos
// "propios". De arquetipo fijo, o sorteando uno por peso en cada spawn si se
// le dan pesos (ver WeightedEnemyType). No es un Entity (no ocupa espacio, no
// bloquea el paso, no tiene HP), pero sí dibuja un marcador simple en el
// suelo -- ver Draw() -- para que el jugador sepa de dónde salen los enemigos.
class Spawner {
public:
    // weightedTypes vacío = spawner clásico, siempre enemyType.
    Spawner(Vector3 position, std::string enemyType, float spawnInterval, int maxEnemies,
            const std::vector<std::unique_ptr<Entity>>* obstacles, Shader shader,
            std::vector<WeightedEnemyType> weightedTypes = {});

    // Empuja un Enemy nuevo a activeEnemies cuando toca -- es la misma lista
    // que dibuja/actualiza Application (m_level.enemies), así el recién
    // nacido entra en el resto del juego sin ningún camino aparte.
    void Update(float dt, std::vector<std::unique_ptr<Enemy>>& activeEnemies);

    // Círculo plano + anillo en el suelo, en la posición del spawner --
    // Application lo llama junto al resto de entidades del nivel.
    void Draw() const;

    Vector3 GetPosition() const { return m_position; }

    // Reduce el intervalo multiplicándolo por factor (< 1 lo acorta), con un
    // suelo mínimo para no degenerar en spawns cada frame. La usa
    // EndlessDirector para escalar la dificultad del Modo Infinito.
    void ScaleSpawnInterval(float factor);

    // true si este spawner sortea el arquetipo en cada spawn. Solo lo usa
    // Draw() para pintarse distinto -- que el jugador vea de un vistazo de
    // qué bocas puede salir cualquier cosa.
    bool IsRandom() const { return !m_weightedTypes.empty(); }

private:
    // Arquetipo de ESTE spawn: m_enemyType si no hay pesos, o uno sorteado
    // entre m_weightedTypes proporcionalmente a su peso.
    std::string PickEnemyType() const;

    Vector3 m_position;
    std::string m_enemyType;
    std::vector<WeightedEnemyType> m_weightedTypes;
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

    // Cupo GLOBAL de Buffers vivos, no por spawner: se cuenta sobre la lista
    // entera de enemigos activos. Un Buffer no ataca, solo acelera al resto,
    // así que varios a la vez no añaden amenaza legible -- multiplican la
    // velocidad de toda la horda hasta volverla imposible de leer. Con dos ya
    // se nota el efecto sin que la partida se descontrole.
    static constexpr int kMaxLiveBuffers = 2;
};
