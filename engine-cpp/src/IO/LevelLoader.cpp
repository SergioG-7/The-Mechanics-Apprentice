#include "LevelLoader.h"
#include "../Entities/Obstacle.h"
#include "../Entities/Cylinder.h"
#include "../Entities/EnemyFactory.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace {

Vector3 ParseVector3(const json& node) {
    // .value(), no .at(): un vector al que le falte una componente (JSON
    // editado a mano) cae a 0 en ese eje en vez de lanzar. Serializado por el
    // editor C# nunca pasa -- Vector3Data siempre escribe las tres.
    return Vector3{
        node.value("x", 0.0f),
        node.value("y", 0.0f),
        node.value("z", 0.0f)
    };
}

// Vector3 de una clave OPCIONAL del nodo. Si la clave falta o no es un
// objeto, devuelve fallback en vez de lanzar. Es el punto único por el que
// pasan todas las posiciones/tamaños del nivel, así que ninguna entrada mal
// formada puede tirar abajo la carga entera.
Vector3 ParseVector3Field(const json& node, const char* key, Vector3 fallback) {
    if (!node.contains(key) || !node.at(key).is_object()) return fallback;
    return ParseVector3(node.at(key));
}

// Recorre un array opcional del nivel aplicando parse a cada elemento, con
// try/catch POR ELEMENTO. Antes, un solo campo mal escrito en una entrada
// hacía saltar el catch global de LoadFromFile y el nivel ENTERO volvía
// vacío -- Application lo interpretaba como "nivel no disponible" y rebotaba
// al menú, sin pista de qué entrada de qué lista era la culpable. Ahora se
// salta esa entrada con un aviso que la identifica y el resto del nivel carga.
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

// "box" (por defecto si falta "type", para niveles de antes de esta fase) o
// "cylinder". Un obstáculo box acepta "size" (dimensión completa, el formato
// nuevo) o, si falta, el "halfExtents" directo de antes -- y si no hay
// ninguno de los dos, 1.0/1.0/1.0 tal como pide el diseño. Ambos tipos
// terminan en la MISMA lista (LevelData::obstacles) porque los dos son
// Entity con AABB; ver el comentario en LevelData.
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
    return std::make_unique<Obstacle>(position, Vector3{ 0.5f, 0.5f, 0.5f }); // "size" 1,1,1 sin declarar
}

// Construye el Enemy directamente con los stats propios de su entrada en el
// JSON del nivel, sin pasar por EnemyFactory. Camino usado tanto por
// type == "Default" como por un type que no coincide con ningún arquetipo de
// enemy_variants.json (variante desconocida: mejor un enemigo con sus stats
// tal cual que perderlo o abortar la carga entera del nivel).
//
// Los stats usan .value(...) con default, no .at(...): un campo suelto que
// falte en UNA entrada (typo, JSON editado a mano) no debe tirar abajo la
// carga del nivel ENTERO -- antes de este cambio, cualquier .at() ausente
// aquí lanzaba, el catch de LoadFromFile lo atrapaba, y TODO el nivel volvía
// vacío (Application lo interpretaba como "nivel no disponible" y volvía al
// menú, sin pista de qué campo faltaba en qué enemigo).
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

        // El jugador es el ÚNICO campo obligatorio: sin él no hay partida que
        // construir, así que su ausencia sí debe abortar la carga (Application
        // lo detecta por level.player == nullptr y vuelve al menú). Sus stats,
        // en cambio, tienen defaults como todo lo demás.
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
            // Opcional (no .at): un enemigo estático sin patrolRoute es
            // válido, y antes su ausencia tiraba abajo el nivel entero.
            std::vector<Vector3> patrolRoute;
            if (n.contains("patrolRoute") && n.at("patrolRoute").is_array()) {
                for (const json& p : n.at("patrolRoute")) patrolRoute.push_back(ParseVector3(p));
            }

            // "type" es opcional por compatibilidad con niveles exportados
            // antes de la Fase 3 (sin el campo, se comportaban todos como
            // "Default": stats propios, sin pasar por EnemyFactory).
            std::string type = n.value("type", std::string("Default"));

            std::unique_ptr<Enemy> enemy;
            if (type == "Default") {
                enemy = BuildEnemyFromOwnStats(n, std::move(patrolRoute));
            } else {
                // Copia (no move) de patrolRoute: si el type no resuelve en el
                // factory, hace falta intacta para el fallback de abajo.
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

            // "weights": { "Runner": 5, "Tank": 1, ... } -- opcional. Si está,
            // el spawner sortea arquetipo en cada spawn; si no, se comporta
            // como siempre (enemyType fijo), que es lo que hace que los
            // niveles ya existentes no cambien.
            if (n.contains("weights") && n.at("weights").is_object()) {
                for (const auto& [name, weight] : n.at("weights").items()) {
                    if (!weight.is_number_integer()) continue; // un peso no numérico se ignora, no rompe el spawner
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

        // !is_null() a propósito: System.Text.Json puede serializar una
        // propiedad C# nula como `"door": null` en vez de omitir la clave.
        // contains() por sí solo daría true y el parseo de abajo trabajaría
        // sobre un null en vez de sobre un objeto.
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
