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
    // Todos los pesos a 0 (o negativos) en el JSON: en vez de dividir entre
    // cero o no generar nunca nada, se cae al arquetipo fijo de siempre.
    if (total <= 0) return m_enemyType;

    int roll = GetRandomValue(1, total);
    for (const WeightedEnemyType& entry : m_weightedTypes) {
        if (entry.weight <= 0) continue;
        roll -= entry.weight;
        if (roll <= 0) return entry.name;
    }
    return m_weightedTypes.back().name; // inalcanzable salvo redondeo raro; mejor que devolver vacío
}

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

    std::string type = PickEnemyType();

    // Cupo de Buffers: se cuenta sobre activeEnemies (la lista global, no
    // solo los propios de este spawner), así que también cuentan los Buffers
    // colocados a mano en el nivel. Se pierde el turno de spawn en vez de
    // sortear otro arquetipo: reintentar sesgaría los pesos de forma difícil
    // de razonar, y saltarse un ciclo es un efecto claro y acotado.
    if (EnemyFactory::GetBehavior(type) == EnemyBehavior::Buffer) {
        int liveBuffers = 0;
        for (const auto& other : activeEnemies) {
            if (other->IsAlive() && other->GetBehavior() == EnemyBehavior::Buffer) liveBuffers++;
        }
        if (liveBuffers >= kMaxLiveBuffers) return;
    }

    // Ruta de un solo punto hacia el centro del mapa: sin ella, un enemigo
    // generado lejos del jugador se queda plantado en su esquina (patrolRoute
    // vacía = no patrulla, ver Enemy::UpdatePatrol) hasta que alguien entre
    // por casualidad en su radio de visión -- que en Infinito, con spawners
    // repartidos por el borde del mapa, podía no pasar nunca. Al llegar al
    // centro se queda ahí (una ruta de un punto no tiene a dónde más ir), con
    // muchas más posibilidades de cruzarse con el jugador de camino.
    auto enemy = EnemyFactory::CreateEnemy(type, m_position, { Vector3{ 0.0f, 0.0f, 0.0f } });
    if (!enemy) return; // variante desconocida; EnemyFactory ya lo avisó por log

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
    // Disco plano + anillo, mismo tono (Magenta) que su marcador en el
    // editor de niveles -- a ras de suelo (0.02f, como la sombra falsa de
    // Enemy) para no competir en altura con nada que camine por encima.
    // Uno aleatorio se pinta violeta y con un anillo exterior extra girando,
    // para que se distinga de un spawner de arquetipo fijo antes de que
    // salga nada por él.
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
