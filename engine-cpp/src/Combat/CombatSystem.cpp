#include "CombatSystem.h"
#include "CollisionMath.h"
#include "../Entities/Entity.h"
#include "../Entities/Player.h"
#include "../Entities/Enemy.h"
#include "../Entities/ExplosiveBarrel.h"
#include "../Entities/Hazard.h"
#include <algorithm>

Hitbox CombatSystem::BuildMeleeHitbox(Vector3 origin, Vector3 direction, float damage, float knockbackForce,
                                       float reach, float halfExtent, float duration) {
    Vector3 center{ origin.x + direction.x * reach, origin.y, origin.z + direction.z * reach };

    Hitbox hitbox;
    hitbox.box = BoundingBox{
        Vector3{ center.x - halfExtent, center.y - halfExtent, center.z - halfExtent },
        Vector3{ center.x + halfExtent, center.y + halfExtent, center.z + halfExtent }
    };
    hitbox.damage = damage;
    hitbox.knockbackDir = Vector3{ direction.x * knockbackForce, 0.0f, direction.z * knockbackForce };
    hitbox.remainingTime = duration;
    return hitbox;
}

std::vector<MeleeHitResult> CombatSystem::ResolveMeleeAttack(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies,
                                                               const std::vector<std::unique_ptr<Entity>>& obstacles) {
    std::vector<MeleeHitResult> hits;

    const Hitbox* hitbox = player.GetActiveHitbox();
    if (!hitbox) return hits;

    Vector3 origin = player.GetPosition();

    // Cleave: recorre TODOS los enemigos, sin cortar en el primer solape --
    // antes paraba ahí (orden de spawn, no distancia) y el resto de
    // enemigos dentro del área de ataque quedaban intactos.
    for (auto& enemy : enemies) {
        if (!enemy->IsAlive() || !CollisionMath::AABBIntersects(enemy->GetBoundingBox(), hitbox->box)) continue;

        // Línea de visión: un Obstacle entre el jugador y ESTE enemigo
        // descarta el golpe solo para él -- el resto del cleave sigue
        // aplicándose con normalidad a quien sí tenga línea libre.
        bool blocked = false;
        for (const auto& obstacle : obstacles) {
            if (CollisionMath::SegmentIntersectsBoxXZ(origin, enemy->GetPosition(), obstacle->GetBoundingBox())) {
                blocked = true;
                break;
            }
        }
        if (blocked) continue;

        MeleeHitResult result;
        result.impactPoint = enemy->GetPosition();
        result.impactPoint.y += 1.0f; // altura de pecho, mismo offset que la barra de HP flotante del HUD
        result.hitEnemy = enemy.get();

        // Shielder golpeado de frente: el golpe se registra (juice, cierre de
        // la ventana de hitbox) pero no aplica daño. Rodearlo es la respuesta.
        if (enemy->BlocksAttackFrom(origin)) {
            result.blocked = true;
        } else {
            enemy->TakeDamage(hitbox->damage, hitbox->knockbackDir);
        }

        hits.push_back(result);
    }

    // La hitbox se cierra una sola vez, tras procesar a todos -- es el mismo
    // swing, no uno por enemigo golpeado. Si no golpeó a nadie, se deja
    // abierta para que siga probando en los frames siguientes hasta que
    // expire por tiempo (ver Hitbox::remainingTime).
    if (!hits.empty()) player.CloseAttackHitbox();

    return hits;
}

std::optional<Vector3> CombatSystem::ResolveMeleeAttackOnBarrels(Player& player, std::vector<std::unique_ptr<ExplosiveBarrel>>& barrels) {
    const Hitbox* hitbox = player.GetActiveHitbox();
    if (!hitbox) return std::nullopt;

    for (auto& barrel : barrels) {
        if (!barrel->HasExploded() && CollisionMath::AABBIntersects(barrel->GetBoundingBox(), hitbox->box)) {
            barrel->TakeDamage(hitbox->damage, hitbox->knockbackDir);
            player.CloseAttackHitbox();
            return barrel->GetPosition();
        }
    }
    return std::nullopt;
}

void CombatSystem::ResolveEnemyAttacks(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies) {
    for (auto& enemy : enemies) {
        if (!enemy->IsAlive()) continue;

        const Hitbox* hitbox = enemy->GetActiveHitbox();
        if (!hitbox) continue;

        if (CollisionMath::AABBIntersects(player.GetBoundingBox(), hitbox->box)) {
            player.TakeDamage(hitbox->damage, hitbox->knockbackDir);
            enemy->CloseAttackHitbox();
        }
    }
}

void CombatSystem::ApplyAreaDamage(Vector3 center, float radius, float damage, Player& player,
                                    std::vector<std::unique_ptr<Enemy>>& enemies,
                                    std::vector<std::unique_ptr<ExplosiveBarrel>>& barrels) {
    constexpr float kAreaKnockbackForce = 7.0f;

    if (CollisionMath::IsWithinRadius(player.GetPosition(), center, radius)) {
        Vector3 dir = CollisionMath::Normalize2D(Vector3{
            player.GetPosition().x - center.x, 0.0f, player.GetPosition().z - center.z });
        player.TakeDamage(damage, Vector3{ dir.x * kAreaKnockbackForce, 0.0f, dir.z * kAreaKnockbackForce });
    }

    // Incluye al propio emisor si está en la lista (p.ej. el Kamikaze que
    // acaba de detonar): fuego amigo real, no un caso a excluir a mano.
    for (auto& enemy : enemies) {
        if (!enemy->IsAlive()) continue;
        if (!CollisionMath::IsWithinRadius(enemy->GetPosition(), center, radius)) continue;

        Vector3 dir = CollisionMath::Normalize2D(Vector3{
            enemy->GetPosition().x - center.x, 0.0f, enemy->GetPosition().z - center.z });
        enemy->TakeDamage(damage, Vector3{ dir.x * kAreaKnockbackForce, 0.0f, dir.z * kAreaKnockbackForce });
    }

    // Mismo criterio que con los Enemy: un barril ya explotado (incluido el
    // propio emisor, si esta llamada viene de su propia explosión) no puede
    // volver a recibir daño -- HasExploded() es una puerta de una sola vía.
    for (auto& barrel : barrels) {
        if (barrel->HasExploded()) continue;
        if (!CollisionMath::IsWithinRadius(barrel->GetPosition(), center, radius)) continue;

        Vector3 dir = CollisionMath::Normalize2D(Vector3{
            barrel->GetPosition().x - center.x, 0.0f, barrel->GetPosition().z - center.z });
        barrel->TakeDamage(damage, Vector3{ dir.x * kAreaKnockbackForce, 0.0f, dir.z * kAreaKnockbackForce });
    }
}

void CombatSystem::UpdateProjectiles(float dt, std::vector<Projectile>& projectiles, Player& player,
                                      const std::vector<std::unique_ptr<Entity>>& obstacles,
                                      std::vector<std::unique_ptr<ExplosiveBarrel>>& barrels) {
    constexpr float kProjectileKnockbackForce = 4.0f;

    for (Projectile& projectile : projectiles) {
        projectile.position.x += projectile.velocity.x * dt;
        projectile.position.y += projectile.velocity.y * dt;
        projectile.position.z += projectile.velocity.z * dt;
        projectile.lifetime -= dt;

        BoundingBox projectileBox{
            Vector3{ projectile.position.x - Projectile::kRadius, projectile.position.y - Projectile::kRadius, projectile.position.z - Projectile::kRadius },
            Vector3{ projectile.position.x + Projectile::kRadius, projectile.position.y + Projectile::kRadius, projectile.position.z + Projectile::kRadius }
        };

        // Contra un obstáculo primero: se destruye sin atravesarlo ni dañar
        // a lo que pueda estar cubierto detrás.
        bool hitObstacle = false;
        for (const auto& obstacle : obstacles) {
            if (CollisionMath::AABBIntersects(projectileBox, obstacle->GetBoundingBox())) {
                hitObstacle = true;
                break;
            }
        }

        // Contra un barril, solo si no hubo obstáculo antes: aplica daño de
        // verdad (no solo destruye el proyectil) para que un Spitter pueda
        // encadenar la explosión de un barril cercano.
        bool hitBarrel = false;
        if (!hitObstacle) {
            for (auto& barrel : barrels) {
                if (!barrel->HasExploded() && CollisionMath::AABBIntersects(projectileBox, barrel->GetBoundingBox())) {
                    Vector3 dir = CollisionMath::Normalize2D(projectile.velocity);
                    barrel->TakeDamage(projectile.damage, Vector3{ dir.x * kProjectileKnockbackForce, 0.0f, dir.z * kProjectileKnockbackForce });
                    hitBarrel = true;
                    break;
                }
            }
        }

        if (hitObstacle || hitBarrel) {
            projectile.lifetime = 0.0f;
        } else if (CollisionMath::AABBIntersects(projectileBox, player.GetBoundingBox())) {
            Vector3 dir = CollisionMath::Normalize2D(projectile.velocity);
            player.TakeDamage(projectile.damage, Vector3{ dir.x * kProjectileKnockbackForce, 0.0f, dir.z * kProjectileKnockbackForce });
            projectile.lifetime = 0.0f; // marca para el erase-remove de abajo
        }
    }

    projectiles.erase(
        std::remove_if(projectiles.begin(), projectiles.end(),
                        [](const Projectile& p) { return p.lifetime <= 0.0f; }),
        projectiles.end());
}

void CombatSystem::ApplyHazardDamage(std::vector<std::unique_ptr<Hazard>>& hazards, Player& player) {
    constexpr float kHazardKnockbackForce = 3.0f;

    for (auto& hazard : hazards) {
        if (!hazard->ConsumeTick()) continue;
        if (!CollisionMath::AABBIntersects(player.GetBoundingBox(), hazard->GetBoundingBox())) continue;

        Vector3 dir = CollisionMath::Normalize2D(Vector3{
            player.GetPosition().x - hazard->GetPosition().x, 0.0f, player.GetPosition().z - hazard->GetPosition().z });
        player.TakeDamage(hazard->GetDamagePerTick(), Vector3{ dir.x * kHazardKnockbackForce, 0.0f, dir.z * kHazardKnockbackForce });
    }
}

void CombatSystem::UpdateMudPuddles(float dt, std::vector<MudPuddle>& puddles, Player& player) {
    for (MudPuddle& puddle : puddles) {
        puddle.lifetime -= dt;

        // Se reaplica cada frame que el jugador siga dentro: al salir, el
        // lastre se le agota kSlowDuration después, no de golpe -- así se
        // nota el "chapoteo" al escapar en vez de recuperar la velocidad en
        // el mismo píxel en que sales del charco.
        if (CollisionMath::IsWithinRadius(player.GetPosition(), puddle.position, MudPuddle::kRadius)) {
            player.ApplySlow(MudPuddle::kSlowDuration, MudPuddle::kSlowMultiplier);
        }
    }

    puddles.erase(
        std::remove_if(puddles.begin(), puddles.end(),
                        [](const MudPuddle& p) { return p.lifetime <= 0.0f; }),
        puddles.end());
}
