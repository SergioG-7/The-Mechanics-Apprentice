#pragma once
#include "raylib.h"
#include "Enemy.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Crea enemigos a partir de arquetipos ("Tank", "Runner"...) definidos en enemy_variants.json.
class EnemyFactory {
public:
    // Crea un enemigo del arquetipo indicado. Devuelve nullptr si no existe.
    static std::unique_ptr<Enemy> CreateEnemy(const std::string& variantName, Vector3 position,
                                               std::vector<Vector3> patrolRoute = {});

    // Consulta el comportamiento de un arquetipo sin llegar a crear el enemigo.
    static EnemyBehavior GetBehavior(const std::string& variantName);

private:
    struct EnemyVariant {
        float maxHP = 20.0f;
        float speed = 2.0f;
        float scale = 1.0f;
        float attackDamage = 5.0f;
        float visionRadius = 6.0f;
        EnemyBehavior behavior = EnemyBehavior::Melee;
        Color tint = WHITE;
        float turnRateDegPerSec = 0.0f;
    };

    // Carga y cachea las variantes desde el JSON la primera vez que se usan.
    static const std::unordered_map<std::string, EnemyVariant>& LoadVariants();
};
