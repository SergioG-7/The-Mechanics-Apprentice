#pragma once
#include "raylib.h"
#include <string>
#include <vector>
#include <memory>
#include "../Entities/Entity.h"
#include "../Entities/Player.h"
#include "../Entities/Enemy.h"
#include "../Entities/Gear.h"
#include "../Entities/Door.h"
#include "../Entities/HealthKit.h"
#include "../Entities/ExplosiveBarrel.h"
#include "../Entities/Hazard.h"
#include "../Entities/PowerUp.h"
#include "../Entities/ElectricTile.h"
#include "../Entities/Spawner.h"

// Datos de un spawner tal como vienen del JSON de nivel.
struct SpawnerData {
    Vector3 position{};
    std::string enemyType;
    float interval = 4.0f;
    int maxEnemies = 3;
    std::vector<WeightedEnemyType> weightedTypes; // vacío = arquetipo fijo
};

// Todo lo que compone un nivel cargado: jugador, enemigos y el resto de entidades.
struct LevelData {
    std::unique_ptr<Player> player;
    std::vector<std::unique_ptr<Enemy>> enemies;

    // Geometría estática que bloquea el movimiento (obstáculos y pilares).
    std::vector<std::unique_ptr<Entity>> obstacles;

    std::vector<std::unique_ptr<Gear>> gears;
    std::unique_ptr<Door> door; // nullptr si el nivel no tiene puerta
    std::vector<SpawnerData> spawners;
    std::vector<std::unique_ptr<HealthKit>> healthKits;
    std::vector<std::unique_ptr<ExplosiveBarrel>> barrels;
    std::vector<std::unique_ptr<PowerUp>> powerUps;

    // Trampas de suelo que dañan por tick, sin bloquear el paso.
    std::vector<std::unique_ptr<Hazard>> hazards;

    // Baldosas eléctricas: tampoco bloquean el paso, pero dañan también a los enemigos.
    std::vector<std::unique_ptr<ElectricTile>> electricTiles;
};

class LevelLoader {
public:
    static LevelData LoadFromFile(const std::string& jsonPath);
};
