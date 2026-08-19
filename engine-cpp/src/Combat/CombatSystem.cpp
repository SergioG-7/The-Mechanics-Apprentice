#include "CombatSystem.h"
#include "CollisionMath.h"
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

std::optional<MeleeHitResult> CombatSystem::ResolveMeleeAttack(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies) {
    const Hitbox* hitbox = player.GetActiveHitbox();
    if (!hitbox) return std::nullopt;

    // Un solo enemigo por golpe: para en el primer solape de la lista
    // (orden de spawn, no distancia), no reparte daño en área.
    for (auto& enemy : enemies) {
        if (enemy->IsAlive() && CollisionMath::AABBIntersects(enemy->GetBoundingBox(), hitbox->box)) {
            enemy->TakeDamage(hitbox->damage, hitbox->knockbackDir);
            player.CloseAttackHitbox();

            MeleeHitResult result;
            result.impactPoint = enemy->GetPosition();
            result.impactPoint.y += 1.0f; // altura de pecho, mismo offset que la barra de HP flotante del HUD
            result.hitEnemy = enemy.get();
            return result;
        }
    }
    return std::nullopt;
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
                                    std::vector<std::unique_ptr<Enemy>>& enemies) {
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
}

void CombatSystem::UpdateProjectiles(float dt, std::vector<Projectile>& projectiles, Player& player) {
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

        if (CollisionMath::AABBIntersects(projectileBox, player.GetBoundingBox())) {
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

void CombatSystem::ApplyHazardDamage(std::vector<std::unique_ptr<Hazard>>& hazards, Player& player,
                                      std::vector<std::unique_ptr<Enemy>>& enemies) {
    constexpr float kHazardKnockbackForce = 3.0f;

    for (auto& hazard : hazards) {
        if (!hazard->ConsumeTick()) continue;

        if (CollisionMath::AABBIntersects(player.GetBoundingBox(), hazard->GetBoundingBox())) {
            Vector3 dir = CollisionMath::Normalize2D(Vector3{
                player.GetPosition().x - hazard->GetPosition().x, 0.0f, player.GetPosition().z - hazard->GetPosition().z });
            player.TakeDamage(hazard->GetDamagePerTick(), Vector3{ dir.x * kHazardKnockbackForce, 0.0f, dir.z * kHazardKnockbackForce });
        }

        for (auto& enemy : enemies) {
            if (!enemy->IsAlive()) continue;
            if (!CollisionMath::AABBIntersects(enemy->GetBoundingBox(), hazard->GetBoundingBox())) continue;

            Vector3 dir = CollisionMath::Normalize2D(Vector3{
                enemy->GetPosition().x - hazard->GetPosition().x, 0.0f, enemy->GetPosition().z - hazard->GetPosition().z });
            enemy->TakeDamage(hazard->GetDamagePerTick(), Vector3{ dir.x * kHazardKnockbackForce, 0.0f, dir.z * kHazardKnockbackForce });
        }
    }
}
