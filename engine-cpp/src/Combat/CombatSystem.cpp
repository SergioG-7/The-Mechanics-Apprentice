#include "CombatSystem.h"
#include "CollisionMath.h"
#include "../Entities/Entity.h"
#include "../Entities/Player.h"
#include "../Entities/Enemy.h"
#include "../Entities/ExplosiveBarrel.h"
#include "../Entities/Hazard.h"
#include "../Entities/ElectricTile.h"
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
    hitbox.knockbackDir = CollisionMath::ScaleXZ(direction, knockbackForce);
    hitbox.remainingTime = duration;
    return hitbox;
}

std::vector<MeleeHitResult> CombatSystem::ResolveMeleeAttack(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies,
                                                               const std::vector<std::unique_ptr<Entity>>& obstacles) {
    std::vector<MeleeHitResult> hits;

    const Hitbox* hitbox = player.GetActiveHitbox();
    if (!hitbox) return hits;

    Vector3 origin = player.GetPosition();

    // Golpea a todos los enemigos que solapen la hitbox, no solo al primero.
    for (auto& enemy : enemies) {
        if (!enemy->IsAlive() || !CollisionMath::AABBIntersects(enemy->GetBoundingBox(), hitbox->box)) continue;

        // Un obstáculo entre el jugador y este enemigo descarta el golpe solo para él.
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
        result.impactPoint.y += 1.0f; // altura de pecho
        result.hitEnemy = enemy.get();

        // Un Shielder golpeado de frente bloquea el daño, pero el golpe cuenta igual.
        if (enemy->BlocksAttackFrom(origin)) {
            result.blocked = true;
        } else {
            enemy->TakeDamage(hitbox->damage, hitbox->knockbackDir);
        }

        hits.push_back(result);
    }

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
        Vector3 dir = CollisionMath::DirectionXZ(center, player.GetPosition());
        player.TakeDamage(damage, CollisionMath::ScaleXZ(dir, kAreaKnockbackForce));
    }

    // Daña también al enemigo que provocó la explosión, si sigue en la lista.
    for (auto& enemy : enemies) {
        if (!enemy->IsAlive()) continue;
        if (!CollisionMath::IsWithinRadius(enemy->GetPosition(), center, radius)) continue;

        Vector3 dir = CollisionMath::DirectionXZ(center, enemy->GetPosition());
        enemy->TakeDamage(damage, CollisionMath::ScaleXZ(dir, kAreaKnockbackForce));
    }

    for (auto& barrel : barrels) {
        if (barrel->HasExploded()) continue;
        if (!CollisionMath::IsWithinRadius(barrel->GetPosition(), center, radius)) continue;

        Vector3 dir = CollisionMath::DirectionXZ(center, barrel->GetPosition());
        barrel->TakeDamage(damage, CollisionMath::ScaleXZ(dir, kAreaKnockbackForce));
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

        // Un obstáculo destruye el proyectil sin dejarlo pasar.
        bool hitObstacle = false;
        for (const auto& obstacle : obstacles) {
            if (CollisionMath::AABBIntersects(projectileBox, obstacle->GetBoundingBox())) {
                hitObstacle = true;
                break;
            }
        }

        bool hitBarrel = false;
        if (!hitObstacle) {
            for (auto& barrel : barrels) {
                if (!barrel->HasExploded() && CollisionMath::AABBIntersects(projectileBox, barrel->GetBoundingBox())) {
                    Vector3 dir = CollisionMath::Normalize2D(projectile.velocity);
                    barrel->TakeDamage(projectile.damage, CollisionMath::ScaleXZ(dir, kProjectileKnockbackForce));
                    hitBarrel = true;
                    break;
                }
            }
        }

        if (hitObstacle || hitBarrel) {
            projectile.lifetime = 0.0f;
        } else if (CollisionMath::AABBIntersects(projectileBox, player.GetBoundingBox())) {
            Vector3 dir = CollisionMath::Normalize2D(projectile.velocity);
            player.TakeDamage(projectile.damage, CollisionMath::ScaleXZ(dir, kProjectileKnockbackForce));
            projectile.lifetime = 0.0f;
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

        Vector3 dir = CollisionMath::DirectionXZ(hazard->GetPosition(), player.GetPosition());
        player.TakeDamage(hazard->GetDamagePerTick(), CollisionMath::ScaleXZ(dir, kHazardKnockbackForce));
    }
}

void CombatSystem::UpdateMudPuddles(float dt, std::vector<MudPuddle>& puddles, Player& player) {
    for (MudPuddle& puddle : puddles) {
        puddle.lifetime -= dt;

        if (CollisionMath::IsWithinRadius(player.GetPosition(), puddle.position, MudPuddle::kRadius)) {
            player.ApplySlow(MudPuddle::kSlowDuration, MudPuddle::kSlowMultiplier);
        }
    }

    puddles.erase(
        std::remove_if(puddles.begin(), puddles.end(),
                        [](const MudPuddle& p) { return p.lifetime <= 0.0f; }),
        puddles.end());
}

void CombatSystem::ApplyBufferAuras(std::vector<std::unique_ptr<Enemy>>& enemies) {
    for (auto& enemy : enemies) enemy->SetSpeedMultiplier(1.0f); // se recalcula entero cada frame

    for (const auto& buffer : enemies) {
        if (buffer->GetBehavior() != EnemyBehavior::Buffer || !buffer->IsAlive()) continue;

        for (auto& target : enemies) {
            if (target.get() == buffer.get() || !target->IsAlive()) continue;
            if (CollisionMath::IsWithinRadius(target->GetPosition(), buffer->GetPosition(), Enemy::kBufferAuraRadius)) {
                target->SetSpeedMultiplier(Enemy::kBufferSpeedBonus);
            }
        }
    }
}

void CombatSystem::UpdateElectricTiles(float dt, std::vector<std::unique_ptr<ElectricTile>>& tiles,
                                        Player& player, std::vector<std::unique_ptr<Enemy>>& enemies) {
    constexpr float kTileKnockbackForce = 5.0f;

    for (auto& tile : tiles) {
        tile->Update(dt);

        BoundingBox tileBox = tile->GetBoundingBox();
        bool playerOnTile = CollisionMath::AABBIntersects(player.GetBoundingBox(), tileBox);

        // Se arma con el jugador o con cualquier enemigo encima.
        if (playerOnTile) {
            tile->Trigger();
        } else {
            for (const auto& enemy : enemies) {
                if (enemy->IsAlive() && CollisionMath::AABBIntersects(enemy->GetBoundingBox(), tileBox)) {
                    tile->Trigger();
                    break;
                }
            }
        }

        if (!tile->ConsumeDischarge()) continue;

        float damage = tile->GetDamage();
        Vector3 center = tile->GetPosition();

        if (CollisionMath::AABBIntersects(player.GetBoundingBox(), tileBox)) {
            Vector3 dir = CollisionMath::DirectionXZ(center, player.GetPosition());
            player.TakeDamage(damage, CollisionMath::ScaleXZ(dir, kTileKnockbackForce));
        }

        for (auto& enemy : enemies) {
            if (!enemy->IsAlive()) continue;
            if (!CollisionMath::AABBIntersects(enemy->GetBoundingBox(), tileBox)) continue;

            Vector3 dir = CollisionMath::DirectionXZ(center, enemy->GetPosition());
            enemy->TakeDamage(damage, CollisionMath::ScaleXZ(dir, kTileKnockbackForce));
        }
    }
}
