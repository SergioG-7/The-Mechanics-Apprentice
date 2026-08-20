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
    if (name == "shielder") return EnemyBehavior::Shielder;
    if (name == "buffer") return EnemyBehavior::Buffer;
    if (name == "trapper") return EnemyBehavior::Trapper;

    TraceLog(LOG_WARNING, "EnemyFactory: behavior '%s' desconocido, usando 'melee'", name.c_str());
    return EnemyBehavior::Melee;
}

// "tint": [r, g, b] con componentes 0-255. Ausente o mal formado = WHITE
// (sin teñir), mismo criterio de degradar sin reventar que ParseBehavior.
Color ParseTint(const json& node) {
    if (!node.contains("tint")) return WHITE;

    const json& tint = node.at("tint");
    if (!tint.is_array() || tint.size() < 3) {
        TraceLog(LOG_WARNING, "EnemyFactory: 'tint' debe ser un array [r, g, b], ignorado");
        return WHITE;
    }

    auto channel = [&tint](size_t i) {
        int value = tint[i].get<int>();
        if (value < 0) value = 0;
        if (value > 255) value = 255;
        return static_cast<unsigned char>(value);
    };
    return Color{ channel(0), channel(1), channel(2), 255 };
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
                variant.tint = ParseTint(node);
                variant.turnRateDegPerSec = node.value("turnRateDegPerSec", 0.0f);
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
                                    v.speed, v.attackDamage, v.scale, v.behavior, v.tint,
                                    v.turnRateDegPerSec);
}

EnemyBehavior EnemyFactory::GetBehavior(const std::string& variantName) {
    const auto& variants = LoadVariants();
    auto it = variants.find(variantName);
    return (it == variants.end()) ? EnemyBehavior::Melee : it->second.behavior;
}
