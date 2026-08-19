#pragma once
#include "raylib.h"
#include "Hitbox.h"
#include "Projectile.h"
#include <vector>
#include <memory>
#include <optional>

class Player;
class Enemy;
class ExplosiveBarrel;
class Hazard;

// Resultado de un golpe conectado: el punto de impacto (juice) y un puntero
// no propietario al Enemy golpeado (para que Application pueda mirar si
// murió con este golpe, p.ej. para el drop de engranaje del Modo Infinito).
// Válido solo dentro del mismo frame -- CombatSystem no es dueño del Enemy.
struct MeleeHitResult {
    Vector3 impactPoint{};
    Enemy* hitEnemy = nullptr;
};

class CombatSystem {
public:
    // Caja de golpe cuerpo a cuerpo centrada 'reach' unidades por delante de
    // origin, en la dirección 'direction' (normalizada) -- Player y Enemy
    // construían la misma BoundingBox campo a campo, solo cambiaban origen,
    // dirección, daño y fuerza de empuje.
    static Hitbox BuildMeleeHitbox(Vector3 origin, Vector3 direction, float damage, float knockbackForce,
                                    float reach = 1.0f, float halfExtent = 0.5f, float duration = 0.15f);

    // Devuelve el resultado del golpe si conecta, o std::nullopt si no había
    // hitbox activa o no golpeó a nadie -- Application lo usa para disparar
    // el juice (hit-stop, shake, chispas) sin que CombatSystem necesite
    // conocer nada de cámara, partículas ni modos de juego.
    static std::optional<MeleeHitResult> ResolveMeleeAttack(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies);

    // Simétrico a ResolveMeleeAttack pero contra barriles en vez de
    // enemigos. Solo se prueba si ResolveMeleeAttack no golpeó ya a un
    // enemigo este frame (la hitbox del Player se cierra en el primer
    // impacto, sea cual sea) -- así un golpe no puede dañar a la vez a un
    // enemigo y a un barril superpuestos.
    static std::optional<Vector3> ResolveMeleeAttackOnBarrels(Player& player, std::vector<std::unique_ptr<ExplosiveBarrel>>& barrels);

    // Simétrico a ResolveMeleeAttack: testea la hitbox activa de cada Enemy
    // vivo contra el AABB del Player. Un enemigo golpea como mucho una vez
    // por ventana de ataque (CloseAttackHitbox tras el impacto), igual que
    // el Player.
    static void ResolveEnemyAttacks(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies);

    // Daño en área compartido por ExplosiveBarrel y el Kamikaze (Enemy):
    // ninguno de los dos conoce al Player ni a los demás Enemy, así que la
    // aplica quien sí los tiene a mano (Application), a través de aquí.
    static void ApplyAreaDamage(Vector3 center, float radius, float damage, Player& player,
                                 std::vector<std::unique_ptr<Enemy>>& enemies);

    // Mueve cada proyectil, comprueba impacto contra el Player y descarta
    // (erase-remove) los que impactan o expiran por tiempo de vida.
    static void UpdateProjectiles(float dt, std::vector<Projectile>& projectiles, Player& player);

    // Un hazard no bloquea el paso (no vive en obstacles), así que a
    // diferencia del resto de combate esto no se dispara por colisión de
    // movimiento: se prueba una vez por frame contra todo lo que siga dentro
    // de su AABB cuando toca tick (ver Hazard::ConsumeTick). Empuja con un
    // knockback suave hacia afuera del centro -- mismo patrón que ApplyAreaDamage.
    static void ApplyHazardDamage(std::vector<std::unique_ptr<Hazard>>& hazards, Player& player,
                                   std::vector<std::unique_ptr<Enemy>>& enemies);
};
