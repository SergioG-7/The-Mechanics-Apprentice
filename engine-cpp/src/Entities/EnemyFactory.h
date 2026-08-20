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

    // Comportamiento de un arquetipo SIN construirlo. La usa Spawner para
    // saber si lo que va a generar es un Buffer antes de gastarse el spawn
    // (ver el cupo de Buffers vivos). Melee si el nombre no existe -- mismo
    // criterio de degradar sin reventar que el resto del factory.
    static EnemyBehavior GetBehavior(const std::string& variantName);

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
        // Límite de giro en grados/segundo ("turnRateDegPerSec"). 0 (por
        // defecto) = instantáneo, como se ha comportado siempre todo el
        // mundo; solo el Shielder declara un valor real.
        float turnRateDegPerSec = 0.0f;
    };

    // Carga perezosa cacheada en un static local: la primera llamada parsea
    // el JSON, las siguientes reutilizan el mismo mapa.
    static const std::unordered_map<std::string, EnemyVariant>& LoadVariants();
};
