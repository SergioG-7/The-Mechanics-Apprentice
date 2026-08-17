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

// Datos crudos de un Spawner tal como los deja el JSON -- no es el Spawner
// en sí (Entities/Spawner.h), que además necesita la lista de obstáculos y
// el shader del nivel para poder construir enemigos; eso lo resuelve
// Application::LoadLevel a partir de esta struct.
struct SpawnerData {
    Vector3 position{};
    std::string enemyType;
    float interval = 4.0f;
    int maxEnemies = 3;
};

struct LevelData {
    std::unique_ptr<Player> player;
    std::vector<std::unique_ptr<Enemy>> enemies;
    std::vector<std::unique_ptr<Entity>> obstacles;
    std::vector<std::unique_ptr<Gear>> gears;
    std::unique_ptr<Door> door; // nullptr si el nivel no define puerta (campo opcional en el JSON)
    std::vector<SpawnerData> spawners;
    std::vector<std::unique_ptr<HealthKit>> healthKits;
    std::vector<std::unique_ptr<ExplosiveBarrel>> barrels;
};

class LevelLoader {
public:
    static LevelData LoadFromFile(const std::string& jsonPath);
};
