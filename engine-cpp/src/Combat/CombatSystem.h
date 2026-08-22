#pragma once
#include "raylib.h"
#include "Hitbox.h"
#include "Projectile.h"
#include "../Entities/MudPuddle.h"
#include <vector>
#include <memory>
#include <optional>

class Entity;
class Player;
class Enemy;
class ExplosiveBarrel;
class Hazard;
class ElectricTile;

// Resultado de un golpe cuerpo a cuerpo conectado: dónde impactó y a qué enemigo.
struct MeleeHitResult {
    Vector3 impactPoint{};
    Enemy* hitEnemy = nullptr;
    bool blocked = false; // true si un Shielder bloqueó el golpe
};

class CombatSystem {
public:
    // Construye la hitbox de un golpe cuerpo a cuerpo, por delante del origen.
    static Hitbox BuildMeleeHitbox(Vector3 origin, Vector3 direction, float damage, float knockbackForce,
                                    float reach = 1.0f, float halfExtent = 0.5f, float duration = 0.15f);

    // Golpea a todos los enemigos vivos que solapen la hitbox activa del jugador este frame.
    static std::vector<MeleeHitResult> ResolveMeleeAttack(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies,
                                                            const std::vector<std::unique_ptr<Entity>>& obstacles);

    // Igual que ResolveMeleeAttack, pero contra barriles explosivos.
    static std::optional<Vector3> ResolveMeleeAttackOnBarrels(Player& player, std::vector<std::unique_ptr<ExplosiveBarrel>>& barrels);

    // Comprueba si la hitbox activa de algún enemigo golpea al jugador.
    static void ResolveEnemyAttacks(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies);

    // Aplica daño en área a todo lo que esté dentro del radio (explosiones).
    static void ApplyAreaDamage(Vector3 center, float radius, float damage, Player& player,
                                 std::vector<std::unique_ptr<Enemy>>& enemies,
                                 std::vector<std::unique_ptr<ExplosiveBarrel>>& barrels);

    // Mueve los proyectiles y comprueba su impacto contra obstáculos, barriles y el jugador.
    static void UpdateProjectiles(float dt, std::vector<Projectile>& projectiles, Player& player,
                                   const std::vector<std::unique_ptr<Entity>>& obstacles,
                                   std::vector<std::unique_ptr<ExplosiveBarrel>>& barrels);

    // Daña al jugador si sigue sobre una trampa de suelo cuando toca su siguiente tick.
    static void ApplyHazardDamage(std::vector<std::unique_ptr<Hazard>>& hazards, Player& player);

    // Envejece los charcos de lodo y ralentiza al jugador que pise uno.
    static void UpdateMudPuddles(float dt, std::vector<MudPuddle>& puddles, Player& player);

    // Avanza las baldosas eléctricas y descarga sobre quien esté encima cuando toca.
    static void UpdateElectricTiles(float dt, std::vector<std::unique_ptr<ElectricTile>>& tiles,
                                     Player& player, std::vector<std::unique_ptr<Enemy>>& enemies);

    // Recalcula el bonus de velocidad de cada enemigo según qué Buffers cercanos lo cubran.
    static void ApplyBufferAuras(std::vector<std::unique_ptr<Enemy>>& enemies);
};
