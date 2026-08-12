#pragma once
#include <string>
#include <vector>
#include <memory>
#include "../Entities/Entity.h"
#include "../Entities/Player.h"
#include "../Entities/Enemy.h"
#include "../Entities/Gear.h"
#include "../Entities/Door.h"

struct LevelData {
    std::unique_ptr<Player> player;
    std::vector<std::unique_ptr<Enemy>> enemies;
    std::vector<std::unique_ptr<Entity>> obstacles;
    std::vector<std::unique_ptr<Gear>> gears;
    std::unique_ptr<Door> door; // nullptr si el nivel no define puerta (campo opcional en el JSON)
};

class LevelLoader {
public:
    static LevelData LoadFromFile(const std::string& jsonPath);
};
