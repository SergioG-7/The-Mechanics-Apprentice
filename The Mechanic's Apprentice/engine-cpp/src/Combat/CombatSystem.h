#pragma once
#include <vector>
#include <memory>

class Player;
class Enemy;

class CombatSystem {
public:
    static void ResolveMeleeAttack(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies);

    // Simétrico a ResolveMeleeAttack: testea la hitbox activa de cada Enemy
    // vivo contra el AABB del Player. Un enemigo golpea como mucho una vez
    // por ventana de ataque (CloseAttackHitbox tras el impacto), igual que
    // el Player.
    static void ResolveEnemyAttacks(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies);
};
