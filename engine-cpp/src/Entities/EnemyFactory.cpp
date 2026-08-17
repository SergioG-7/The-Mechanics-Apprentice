#include "EnemyFactory.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace {
constexpr const char* kVariantsPath = "assets/data/enemy_variants.json";

// Variante desconocida (typo, JSON viejo) cae a Melee y avisa -- igual que
// el resto de este archivo trata cualquier dato de variante que no cuadra:
// nunca revienta la carga, solo degrada con un warning.
EnemyBehavior ParseBehavior(const std::string& name) {
    if (name == "melee") return EnemyBehavior::Melee;
    if (name == "kamikaze") return EnemyBehavior::Kamikaze;
    if (name == "spitter") return EnemyBehavior::Spitter;

    TraceLog(LOG_WARNING, "EnemyFactory: behavior '%s' desconocido, usando 'melee'", name.c_str());
    return EnemyBehavior::Melee;
}
}

const std::unordered_map<std::string, EnemyFactory::EnemyVariant>& EnemyFactory::LoadVariants() {
    static const std::unordered_map<std::string, EnemyVariant> variants = [] {
        std::unordered_map<std::string, EnemyVariant> result;

        std::ifstream file(kVariantsPath);
        if (!file.is_open()) {
            TraceLog(LOG_WARNING, "EnemyFactory: no se pudo abrir '%s'", kVariantsPath);
            return result;
        }

        try {
            json root;
            file >> root;
            for (const auto& [name, node] : root.at("variants").items()) {
                EnemyVariant variant;
                variant.maxHP = node.at("maxHP").get<float>();
                variant.speed = node.at("speed").get<float>();
                variant.scale = node.at("scale").get<float>();
                variant.attackDamage = node.at("attackDamage").get<float>();
                variant.visionRadius = node.at("visionRadius").get<float>();
                // Opcional: las variantes ya existentes (Tank/Runner) no lo
                // llevan y siguen siendo Melee sin tocarlas.
                variant.behavior = ParseBehavior(node.value("behavior", std::string("melee")));
                result.emplace(name, variant);
            }
        } catch (const json::exception& e) {
            TraceLog(LOG_WARNING, "EnemyFactory: '%s' mal formado (%s)", kVariantsPath, e.what());
        }

        return result;
    }();

    return variants;
}

std::unique_ptr<Enemy> EnemyFactory::CreateEnemy(const std::string& variantName, Vector3 position,
                                                  std::vector<Vector3> patrolRoute) {
    const auto& variants = LoadVariants();
    auto it = variants.find(variantName);
    if (it == variants.end()) {
        TraceLog(LOG_WARNING, "EnemyFactory: variante '%s' no encontrada", variantName.c_str());
        return nullptr;
    }

    const EnemyVariant& v = it->second;
    return std::make_unique<Enemy>(position, v.maxHP, std::move(patrolRoute), v.visionRadius,
                                    v.speed, v.attackDamage, v.scale, v.behavior);
}
