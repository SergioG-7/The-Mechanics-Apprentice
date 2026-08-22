#include "LevelLoader.h"
#include "../Entities/Obstacle.h"
#include "../Entities/Cylinder.h"
#include "../Entities/EnemyFactory.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace {

// Lee un Vector3 del JSON; si falta alguna componente, la deja a 0.
Vector3 ParseVector3(const json& node) {
    return Vector3{
        node.value("x", 0.0f),
        node.value("y", 0.0f),
        node.value("z", 0.0f)
    };
}

// Lee un Vector3 de una clave opcional; si falta, devuelve el valor por defecto.
Vector3 ParseVector3Field(const json& node, const char* key, Vector3 fallback) {
    if (!node.contains(key) || !node.at(key).is_object()) return fallback;
    return ParseVector3(node.at(key));
}

// Recorre un array del nivel aplicando parse a cada elemento; si uno falla, se salta con un aviso.
template <typename ParseFn>
void ParseArray(const json& root, const char* key, ParseFn parse) {
    if (!root.contains(key) || !root.at(key).is_array()) return;

    const json& array = root.at(key);
    for (size_t i = 0; i < array.size(); i++) {
        try {
            parse(array[i]);
        } catch (const json::exception& e) {
            TraceLog(LOG_WARNING, "LevelLoader: '%s'[%d] mal formado, se omite (%s)",
                     key, static_cast<int>(i), e.what());
        }
    }
}

// Construye un obstáculo de tipo "box" o "cylinder" a partir del JSON.
std::unique_ptr<Entity> ParseObstacle(const json& n) {
    Vector3 position = ParseVector3Field(n, "position", Vector3{ 0.0f, 0.0f, 0.0f });
    std::string type = n.value("type", std::string("box"));

    if (type == "cylinder") {
        float radius = n.value("radius", 0.5f);
        float height = n.value("height", 1.0f);
        return std::make_unique<Cylinder>(position, radius, height);
    }

    if (n.contains("size")) {
        Vector3 size = ParseVector3(n.at("size"));
        return std::make_unique<Obstacle>(position, Vector3{ size.x * 0.5f, size.y * 0.5f, size.z * 0.5f });
    }
    if (n.contains("halfExtents")) {
        return std::make_unique<Obstacle>(position, ParseVector3(n.at("halfExtents")));
    }
    return std::make_unique<Obstacle>(position, Vector3{ 0.5f, 0.5f, 0.5f });
}

// Construye un Enemy con los stats propios de su entrada en el JSON, sin pasar por EnemyFactory.
std::unique_ptr<Enemy> BuildEnemyFromOwnStats(const json& n, std::vector<Vector3> patrolRoute) {
    return std::make_unique<Enemy>(
        ParseVector3Field(n, "spawn", Vector3{ 0.0f, 0.0f, 0.0f }),
        n.value("maxHP", 30.0f),
        std::move(patrolRoute),
        n.value("visionRadius", 5.0f),
        n.value("speed", 2.5f),
        n.value("attackDamage", 10.0f));
}

} // namespace

LevelData LevelLoader::LoadFromFile(const std::string& jsonPath) {
    LevelData level;

    std::ifstream file(jsonPath);
    if (!file.is_open()) {
        TraceLog(LOG_WARNING, "LevelLoader: no se pudo abrir '%s'", jsonPath.c_str());
        return level;
    }

    try {
        json root;
        file >> root;

        // El jugador es el único campo obligatorio del nivel.
        const json& playerNode = root.at("player");
        level.player = std::make_unique<Player>(
            ParseVector3Field(playerNode, "spawn", Vector3{ 0.0f, 0.0f, 0.0f }),
            playerNode.value("maxHP", 100.0f),
            playerNode.value("speed", 4.5f),
            playerNode.value("attackDamage", 45.0f));

        ParseArray(root, "obstacles", [&level](const json& n) {
            level.obstacles.push_back(ParseObstacle(n));
        });

        ParseArray(root, "hazards", [&level](const json& n) {
            level.hazards.push_back(std::make_unique<Hazard>(
                ParseVector3Field(n, "position", Vector3{ 0.0f, 0.0f, 0.0f }),
                ParseVector3Field(n, "size", Vector3{ 1.0f, 0.1f, 1.0f }),
                n.value("damagePerTick", 10.0f)));
        });

        ParseArray(root, "enemies", [&level](const json& n) {
            std::vector<Vector3> patrolRoute;
            if (n.contains("patrolRoute") && n.at("patrolRoute").is_array()) {
                for (const json& p : n.at("patrolRoute")) patrolRoute.push_back(ParseVector3(p));
            }

            std::string type = n.value("type", std::string("Default"));

            std::unique_ptr<Enemy> enemy;
            if (type == "Default") {
                enemy = BuildEnemyFromOwnStats(n, std::move(patrolRoute));
            } else {
                enemy = EnemyFactory::CreateEnemy(type, ParseVector3Field(n, "spawn", Vector3{ 0.0f, 0.0f, 0.0f }), patrolRoute);
                if (!enemy) {
                    TraceLog(LOG_WARNING,
                             "LevelLoader: enemigo con type '%s' no encontrado en enemy_variants.json, usando sus propios stats del JSON",
                             type.c_str());
                    enemy = BuildEnemyFromOwnStats(n, std::move(patrolRoute));
                }
            }

            level.enemies.push_back(std::move(enemy));
        });

        ParseArray(root, "spawners", [&level](const json& n) {
            SpawnerData spawner;
            spawner.position = ParseVector3Field(n, "position", Vector3{ 0.0f, 0.0f, 0.0f });
            spawner.enemyType = n.value("enemyType", std::string("Runner"));
            spawner.interval = n.value("interval", 4.0f);
            spawner.maxEnemies = n.value("maxEnemies", 3);

            // "weights" opcional: si está, el spawner sortea arquetipo en cada spawn.
            if (n.contains("weights") && n.at("weights").is_object()) {
                for (const auto& [name, weight] : n.at("weights").items()) {
                    if (!weight.is_number_integer()) continue;
                    spawner.weightedTypes.push_back(WeightedEnemyType{ name, weight.get<int>() });
                }
            }

            level.spawners.push_back(std::move(spawner));
        });

        ParseArray(root, "electricTiles", [&level](const json& n) {
            level.electricTiles.push_back(std::make_unique<ElectricTile>(
                ParseVector3Field(n, "position", Vector3{ 0.0f, 0.0f, 0.0f }),
                ParseVector3Field(n, "size", Vector3{ 2.0f, 0.1f, 2.0f }),
                n.value("damage", 20.0f),
                n.value("cycleInterval", 0.0f)));
        });

        ParseArray(root, "gears", [&level](const json& n) {
            level.gears.push_back(std::make_unique<Gear>(ParseVector3Field(n, "position", Vector3{ 0.0f, 0.0f, 0.0f })));
        });

        ParseArray(root, "healthKits", [&level](const json& n) {
            level.healthKits.push_back(std::make_unique<HealthKit>(ParseVector3Field(n, "position", Vector3{ 0.0f, 0.0f, 0.0f })));
        });

        ParseArray(root, "barrels", [&level](const json& n) {
            level.barrels.push_back(std::make_unique<ExplosiveBarrel>(ParseVector3Field(n, "position", Vector3{ 0.0f, 0.0f, 0.0f })));
        });

        ParseArray(root, "powerUps", [&level](const json& n) {
            level.powerUps.push_back(std::make_unique<PowerUp>(
                ParseVector3Field(n, "position", Vector3{ 0.0f, 0.0f, 0.0f }),
                PowerUp::ParseType(n.value("type", std::string("Overclock")))));
        });

        if (root.contains("door") && root.at("door").is_object()) {
            const json& doorNode = root.at("door");
            level.door = std::make_unique<Door>(
                ParseVector3Field(doorNode, "position", Vector3{ 0.0f, 0.0f, 0.0f }),
                ParseVector3Field(doorNode, "halfExtents", Vector3{ 1.0f, 1.0f, 1.0f }));
        }

        TraceLog(LOG_INFO, "LevelLoader: '%s' cargado -> %d enemigos, %d obstaculos, %d hazards, %d baldosas, %d engranajes, %d spawners, %d botiquines, %d barriles, %d power-ups, puerta %s",
                 jsonPath.c_str(),
                 static_cast<int>(level.enemies.size()),
                 static_cast<int>(level.obstacles.size()),
                 static_cast<int>(level.hazards.size()),
                 static_cast<int>(level.electricTiles.size()),
                 static_cast<int>(level.gears.size()),
                 static_cast<int>(level.spawners.size()),
                 static_cast<int>(level.healthKits.size()),
                 static_cast<int>(level.barrels.size()),
                 static_cast<int>(level.powerUps.size()),
                 level.door ? "si" : "no");

    } catch (const json::exception& e) {
        TraceLog(LOG_WARNING, "LevelLoader: '%s' mal formado (%s)", jsonPath.c_str(), e.what());
        level = LevelData{};
    }

    return level;
}
