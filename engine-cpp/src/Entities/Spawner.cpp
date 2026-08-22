#include "Spawner.h"
#include "EnemyFactory.h"
#include "../Core/Pulse.h"
#include <algorithm>

Spawner::Spawner(Vector3 position, std::string enemyType, float spawnInterval, int maxEnemies,
                  const std::vector<std::unique_ptr<Entity>>* obstacles, Shader shader,
                  std::vector<WeightedEnemyType> weightedTypes)
    : m_position(position), m_enemyType(std::move(enemyType)), m_weightedTypes(std::move(weightedTypes)),
      m_spawnInterval(spawnInterval), m_maxEnemies(maxEnemies), m_timer(spawnInterval),
      m_obstacles(obstacles), m_shader(shader) {}

std::string Spawner::PickEnemyType() const {
    if (m_weightedTypes.empty()) return m_enemyType;

    int total = 0;
    for (const WeightedEnemyType& entry : m_weightedTypes) {
        if (entry.weight > 0) total += entry.weight;
    }
    if (total <= 0) return m_enemyType; // sin pesos válidos, usa el arquetipo fijo

    int roll = GetRandomValue(1, total);
    for (const WeightedEnemyType& entry : m_weightedTypes) {
        if (entry.weight <= 0) continue;
        roll -= entry.weight;
        if (roll <= 0) return entry.name;
    }
    return m_weightedTypes.back().name;
}

void Spawner::Update(float dt, std::vector<std::unique_ptr<Enemy>>& activeEnemies) {
    // Descarta los enemigos propios ya muertos antes de comprobar el tope.
    m_spawnedEnemies.erase(
        std::remove_if(m_spawnedEnemies.begin(), m_spawnedEnemies.end(),
                        [](const Enemy* e) { return !e->IsAlive(); }),
        m_spawnedEnemies.end());

    m_timer -= dt;
    if (m_timer > 0.0f) return;
    m_timer = m_spawnInterval;

    if (static_cast<int>(m_spawnedEnemies.size()) >= m_maxEnemies) return;

    std::string type = PickEnemyType();

    // Si toca generar un Buffer pero ya hay demasiados vivos, se salta este spawn.
    if (EnemyFactory::GetBehavior(type) == EnemyBehavior::Buffer) {
        int liveBuffers = 0;
        for (const auto& other : activeEnemies) {
            if (other->IsAlive() && other->GetBehavior() == EnemyBehavior::Buffer) liveBuffers++;
        }
        if (liveBuffers >= kMaxLiveBuffers) return;
    }

    // El enemigo generado patrulla hacia el centro del mapa en vez de quedarse quieto.
    auto enemy = EnemyFactory::CreateEnemy(type, m_position, { Vector3{ 0.0f, 0.0f, 0.0f } });
    if (!enemy) return; // variante desconocida

    enemy->SetObstacles(m_obstacles);
    enemy->SetShader(m_shader);

    m_spawnedEnemies.push_back(enemy.get());
    activeEnemies.push_back(std::move(enemy));
}

void Spawner::ForgetDestroyedEnemies() {
    m_spawnedEnemies.erase(
        std::remove_if(m_spawnedEnemies.begin(), m_spawnedEnemies.end(),
                        [](const Enemy* e) { return e->IsPendingDestruction(); }),
        m_spawnedEnemies.end());
}

void Spawner::ScaleSpawnInterval(float factor) {
    m_spawnInterval = std::max(kMinSpawnInterval, m_spawnInterval * factor);
}

void Spawner::Draw() const {
    // Disco y anillo en el suelo; uno aleatorio se pinta violeta con un anillo extra girando.
    constexpr float kRadius = 1.0f;
    Vector3 base{ m_position.x, 0.02f, m_position.z };
    Color color = IsRandom() ? Color{ 170, 90, 255, 255 } : MAGENTA;

    DrawCylinder(base, kRadius, kRadius, 0.02f, 24, Fade(color, 0.35f));
    DrawCylinderWires(base, kRadius, kRadius, 0.02f, 24, color);

    if (IsRandom()) {
        float pulse = Pulse::Wave01(static_cast<float>(GetTime()), 2.5f);
        float outer = kRadius * (1.15f + 0.15f * pulse);
        DrawCylinderWires(base, outer, outer, 0.02f, 12, Fade(color, 0.4f + 0.4f * pulse));
    }
}
