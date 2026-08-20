#include "Spawner.h"
#include "EnemyFactory.h"
#include <algorithm>

Spawner::Spawner(Vector3 position, std::string enemyType, float spawnInterval, int maxEnemies,
                  const std::vector<std::unique_ptr<Entity>>* obstacles, Shader shader)
    : m_position(position), m_enemyType(std::move(enemyType)), m_spawnInterval(spawnInterval),
      m_maxEnemies(maxEnemies), m_timer(spawnInterval), m_obstacles(obstacles), m_shader(shader) {}

void Spawner::Update(float dt, std::vector<std::unique_ptr<Enemy>>& activeEnemies) {
    // Olvida primero los punteros de los suyos que ya han muerto -- si no, el
    // cupo se quedaría "lleno" para siempre en cuanto muriera uno de ellos.
    m_spawnedEnemies.erase(
        std::remove_if(m_spawnedEnemies.begin(), m_spawnedEnemies.end(),
                        [](const Enemy* e) { return !e->IsAlive(); }),
        m_spawnedEnemies.end());

    m_timer -= dt;
    if (m_timer > 0.0f) return;
    m_timer = m_spawnInterval;

    if (static_cast<int>(m_spawnedEnemies.size()) >= m_maxEnemies) return;

    // Ruta de un solo punto hacia el centro del mapa: sin ella, un enemigo
    // generado lejos del jugador se queda plantado en su esquina (patrolRoute
    // vacía = no patrulla, ver Enemy::UpdatePatrol) hasta que alguien entre
    // por casualidad en su radio de visión -- que en Infinito, con spawners
    // repartidos por el borde del mapa, podía no pasar nunca. Al llegar al
    // centro se queda ahí (una ruta de un punto no tiene a dónde más ir), con
    // muchas más posibilidades de cruzarse con el jugador de camino.
    auto enemy = EnemyFactory::CreateEnemy(m_enemyType, m_position, { Vector3{ 0.0f, 0.0f, 0.0f } });
    if (!enemy) return; // variante desconocida; EnemyFactory ya lo avisó por log

    enemy->SetObstacles(m_obstacles);
    enemy->SetShader(m_shader);

    m_spawnedEnemies.push_back(enemy.get());
    activeEnemies.push_back(std::move(enemy));
}

void Spawner::ScaleSpawnInterval(float factor) {
    m_spawnInterval = std::max(kMinSpawnInterval, m_spawnInterval * factor);
}

void Spawner::Draw() const {
    // Disco plano + anillo, mismo tono (Magenta) que su marcador en el
    // editor de niveles -- a ras de suelo (0.02f, como la sombra falsa de
    // Enemy) para no competir en altura con nada que camine por encima.
    constexpr float kRadius = 1.0f;
    Vector3 base{ m_position.x, 0.02f, m_position.z };
    DrawCylinder(base, kRadius, kRadius, 0.02f, 24, Fade(MAGENTA, 0.35f));
    DrawCylinderWires(base, kRadius, kRadius, 0.02f, 24, MAGENTA);
}
