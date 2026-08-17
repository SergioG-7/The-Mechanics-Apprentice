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

    auto enemy = EnemyFactory::CreateEnemy(m_enemyType, m_position);
    if (!enemy) return; // variante desconocida; EnemyFactory ya lo avisó por log

    enemy->SetObstacles(m_obstacles);
    enemy->SetShader(m_shader);

    m_spawnedEnemies.push_back(enemy.get());
    activeEnemies.push_back(std::move(enemy));
}

void Spawner::ScaleSpawnInterval(float factor) {
    m_spawnInterval = std::max(kMinSpawnInterval, m_spawnInterval * factor);
}
