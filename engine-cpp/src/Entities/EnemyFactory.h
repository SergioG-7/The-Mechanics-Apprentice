#pragma once
#include "raylib.h"
#include "Enemy.h"
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

// Instancia Enemy a partir de arquetipos con nombre ("Tank", "Runner", ...)
// definidos en assets/data/enemy_variants.json. La usa Spawner para generar
// oleadas sin que cada spawner tenga que conocer los stats de cada variante.
class EnemyFactory {
public:
    // patrolRoute vacía es el caso normal para un enemigo generado por un
    // Spawner: no patrulla, solo reacciona si el jugador entra en su radio
    // de visión (ver Enemy::UpdatePatrol con ruta vacía). Devuelve nullptr si
    // variantName no está en el JSON.
    static std::unique_ptr<Enemy> CreateEnemy(const std::string& variantName, Vector3 position,
                                               std::vector<Vector3> patrolRoute = {});

private:
    struct EnemyVariant {
        float maxHP = 20.0f;
        float speed = 2.0f;
        float scale = 1.0f;
        float attackDamage = 5.0f;
        float visionRadius = 6.0f;
        // "melee" por defecto: las variantes ya existentes (Tank/Runner) no
        // llevan "behavior" en el JSON y deben seguir comportándose igual.
        EnemyBehavior behavior = EnemyBehavior::Melee;
        // Código de color del arquetipo ("tint": [r, g, b] en el JSON).
        // WHITE = sin teñir, el aspecto original del modelo.
        Color tint = WHITE;
    };

    // Carga perezosa cacheada en un static local: la primera llamada parsea
    // el JSON, las siguientes reutilizan el mismo mapa.
    static const std::unordered_map<std::string, EnemyVariant>& LoadVariants();
};
