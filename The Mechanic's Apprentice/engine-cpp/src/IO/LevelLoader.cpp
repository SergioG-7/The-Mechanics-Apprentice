#include "LevelLoader.h"
#include "../Entities/Obstacle.h"
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

                level.enemies.push_back(std::make_unique<Enemy>(
                    ParseVector3(n.at("spawn")),
                    n.at("maxHP").get<float>(),
                    std::move(patrolRoute),
                    n.at("visionRadius").get<float>(),
                    n.at("speed").get<float>(),
                    n.at("attackDamage").get<float>()));
            }
        }

        if (root.contains("gears")) {
            for (const json& n : root.at("gears")) {
                level.gears.push_back(std::make_unique<Gear>(ParseVector3(n.at("position"))));
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

        TraceLog(LOG_INFO, "LevelLoader: '%s' cargado -> %d enemigos, %d obstaculos, %d engranajes, puerta %s",
                 jsonPath.c_str(),
                 static_cast<int>(level.enemies.size()),
                 static_cast<int>(level.obstacles.size()),
                 static_cast<int>(level.gears.size()),
                 level.door ? "si" : "no");

    } catch (const json::exception& e) {
        TraceLog(LOG_WARNING, "LevelLoader: '%s' mal formado (%s)", jsonPath.c_str(), e.what());
        level = LevelData{};
    }

    return level;
}
