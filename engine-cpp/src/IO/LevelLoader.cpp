#include "LevelLoader.h"
#include "../Entities/Obstacle.h"
#include "../Entities/EnemyFactory.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace {

Vector3 ParseVector3(const json& node) {
    return Vector3{
        node.at("x").get<float>(),
        node.at("y").get<float>(),
        node.at("z").get<float>()
    };
}

// Construye el Enemy directamente con los stats propios de su entrada en el
// JSON del nivel, sin pasar por EnemyFactory. Camino usado tanto por
// type == "Default" como por un type que no coincide con ningún arquetipo de
// enemy_variants.json (variante desconocida: mejor un enemigo con sus stats
// tal cual que perderlo o abortar la carga entera del nivel).
std::unique_ptr<Enemy> BuildEnemyFromOwnStats(const json& n, std::vector<Vector3> patrolRoute) {
    return std::make_unique<Enemy>(
        ParseVector3(n.at("spawn")),
        n.at("maxHP").get<float>(),
        std::move(patrolRoute),
        n.at("visionRadius").get<float>(),
        n.at("speed").get<float>(),
        n.at("attackDamage").get<float>());
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

        const json& playerNode = root.at("player");
        level.player = std::make_unique<Player>(
            ParseVector3(playerNode.at("spawn")),
            playerNode.at("maxHP").get<float>(),
            playerNode.at("speed").get<float>(),
            playerNode.at("attackDamage").get<float>());

        if (root.contains("obstacles")) {
            for (const json& n : root.at("obstacles")) {
                level.obstacles.push_back(std::make_unique<Obstacle>(
                    ParseVector3(n.at("position")), ParseVector3(n.at("halfExtents"))));
            }
        }

        if (root.contains("enemies")) {
            for (const json& n : root.at("enemies")) {
                std::vector<Vector3> patrolRoute;
                for (const json& p : n.at("patrolRoute")) patrolRoute.push_back(ParseVector3(p));

                // "type" es opcional por compatibilidad con niveles exportados
                // antes de la Fase 3 (sin el campo, se comportaban todos como
                // "Default": stats propios, sin pasar por EnemyFactory).
                std::string type = n.value("type", std::string("Default"));

                std::unique_ptr<Enemy> enemy;
                if (type == "Default") {
                    enemy = BuildEnemyFromOwnStats(n, std::move(patrolRoute));
                } else {
                    // Copia (no move) de patrolRoute: si el type no resuelve
                    // en el factory, hace falta intacta para el fallback de abajo.
                    enemy = EnemyFactory::CreateEnemy(type, ParseVector3(n.at("spawn")), patrolRoute);
                    if (!enemy) {
                        TraceLog(LOG_WARNING,
                                 "LevelLoader: enemigo con type '%s' no encontrado en enemy_variants.json, usando sus propios stats del JSON",
                                 type.c_str());
                        enemy = BuildEnemyFromOwnStats(n, std::move(patrolRoute));
                    }
                }

                level.enemies.push_back(std::move(enemy));
            }
        }

        if (root.contains("spawners")) {
            for (const json& n : root.at("spawners")) {
                SpawnerData spawner;
                spawner.position = ParseVector3(n.at("position"));
                spawner.enemyType = n.at("enemyType").get<std::string>();
                spawner.interval = n.at("interval").get<float>();
                spawner.maxEnemies = n.at("maxEnemies").get<int>();
                level.spawners.push_back(std::move(spawner));
            }
        }

        if (root.contains("gears")) {
            for (const json& n : root.at("gears")) {
                level.gears.push_back(std::make_unique<Gear>(ParseVector3(n.at("position"))));
            }
        }

        if (root.contains("healthKits")) {
            for (const json& n : root.at("healthKits")) {
                level.healthKits.push_back(std::make_unique<HealthKit>(ParseVector3(n.at("position"))));
            }
        }

        if (root.contains("barrels")) {
            for (const json& n : root.at("barrels")) {
                level.barrels.push_back(std::make_unique<ExplosiveBarrel>(ParseVector3(n.at("position"))));
            }
        }

        // !is_null() a propósito: System.Text.Json puede serializar una
        // propiedad C# nula como `"door": null` en vez de omitir la clave.
        // contains() por sí solo daría true y el .at("position") de abajo
        // reventaría contra un valor null en vez de un objeto.
        if (root.contains("door") && !root.at("door").is_null()) {
            const json& doorNode = root.at("door");
            level.door = std::make_unique<Door>(
                ParseVector3(doorNode.at("position")),
                ParseVector3(doorNode.at("halfExtents")));
        }

        TraceLog(LOG_INFO, "LevelLoader: '%s' cargado -> %d enemigos, %d obstaculos, %d engranajes, %d spawners, %d botiquines, %d barriles, puerta %s",
                 jsonPath.c_str(),
                 static_cast<int>(level.enemies.size()),
                 static_cast<int>(level.obstacles.size()),
                 static_cast<int>(level.gears.size()),
                 static_cast<int>(level.spawners.size()),
                 static_cast<int>(level.healthKits.size()),
                 static_cast<int>(level.barrels.size()),
                 level.door ? "si" : "no");

    } catch (const json::exception& e) {
        TraceLog(LOG_WARNING, "LevelLoader: '%s' mal formado (%s)", jsonPath.c_str(), e.what());
        level = LevelData{};
    }

    return level;
}
