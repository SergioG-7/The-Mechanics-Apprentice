#pragma once
#include "Entity.h"
#include <vector>
#include <memory>

// Entity que puede recibir daño y ser empujado. Player y Enemy heredan de aquí.
class Actor : public Entity {
public:
    Actor(Vector3 position, float maxHP, Vector3 halfExtents = { 0.5f, 0.5f, 0.5f });

    virtual void TakeDamage(float amount, Vector3 knockbackDir);
    void Heal(float amount);
    bool IsAlive() const { return m_hp > 0.0f; }
    float GetHP() const { return m_hp; }
    float GetMaxHP() const { return m_maxHP; }

    // Guarda la lista de obstáculos del nivel para comprobar colisiones al moverse.
    void SetObstacles(const std::vector<std::unique_ptr<Entity>>* obstacles) { m_obstacles = obstacles; }

protected:
    // Aplica el empuje de knockback restante, con desgaste, moviendo al actor.
    void ApplyKnockback(float dt);

    // Mueve al actor comprobando colisión contra los obstáculos del nivel.
    void TryMoveAgainstObstacles(Vector3 delta);

    float m_hp;
    float m_maxHP;
    Vector3 m_knockbackVelocity{};
    static constexpr float kKnockbackDrag = 8.0f;

private:
    const std::vector<std::unique_ptr<Entity>>* m_obstacles = nullptr;
};
