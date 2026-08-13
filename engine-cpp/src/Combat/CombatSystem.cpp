#include "CombatSystem.h"
#include "CollisionMath.h"
#include "../Entities/Player.h"
#include "../Entities/Enemy.h"

void CombatSystem::ResolveMeleeAttack(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies) {
    const Hitbox* hitbox = player.GetActiveHitbox();
    if (!hitbox) return;

    // Un solo enemigo por golpe: para en el primer solape de la lista
    // (orden de spawn, no distancia), no reparte daño en área.
    for (auto& enemy : enemies) {
        if (enemy->IsAlive() && CollisionMath::AABBIntersects(enemy->GetBoundingBox(), hitbox->box)) {
            enemy->TakeDamage(hitbox->damage, hitbox->knockbackDir);
            player.CloseAttackHitbox();
            break;
        }
    }
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
