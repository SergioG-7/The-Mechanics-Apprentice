#pragma once
#include "Entity.h"
#include <vector>
#include <memory>

// Entity que puede recibir daño y ser empujado (knockback). Player y Enemy
// heredan de aquí; Obstacle se queda en Entity porque es estático e indestructible.
class Actor : public Entity {
public:
    Actor(Vector3 position, float maxHP, Vector3 halfExtents = { 0.5f, 0.5f, 0.5f });

    virtual void TakeDamage(float amount, Vector3 knockbackDir);
    bool IsAlive() const { return m_hp > 0.0f; }
    float GetHP() const { return m_hp; }
    float GetMaxHP() const { return m_maxHP; }

    // Referencia no propietaria a los obstáculos del nivel; Application la
    // fija una vez tras cargar el LevelData. Player/Enemy la usan en su
    // propio movimiento antes de comprometer una nueva posición.
    void SetObstacles(const std::vector<std::unique_ptr<Entity>>* obstacles) { m_obstacles = obstacles; }

protected:
    void ApplyKnockback(float dt);

    // true si un AABB centrado en candidatePosition (con m_halfExtents de
    // este Actor) solaparía con algún obstáculo. Player/Enemy la llaman antes
    // de aplicar su movimiento; si da true, cancelan ese paso.
    bool WouldCollideWithObstacles(Vector3 candidatePosition) const;

    float m_hp;
    float m_maxHP;
    Vector3 m_knockbackVelocity{};
    static constexpr float kKnockbackDrag = 8.0f;

private:
    const std::vector<std::unique_ptr<Entity>>* m_obstacles = nullptr;
};
