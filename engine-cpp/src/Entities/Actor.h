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
    void Heal(float amount);
    bool IsAlive() const { return m_hp > 0.0f; }
    float GetHP() const { return m_hp; }
    float GetMaxHP() const { return m_maxHP; }

    // Referencia no propietaria a los obstáculos del nivel; Application la
    // fija una vez tras cargar el LevelData. Player/Enemy la usan en su
    // propio movimiento antes de comprometer una nueva posición.
    void SetObstacles(const std::vector<std::unique_ptr<Entity>>* obstacles) { m_obstacles = obstacles; }

protected:
    void ApplyKnockback(float dt);

    // Envuelve Entity::TryMove con la lista de obstáculos del nivel; Player y
    // Enemy la llaman para moverse deslizando por las paredes en vez de
    // quedarse parados en seco al chocar.
    void TryMoveAgainstObstacles(Vector3 delta);

    float m_hp;
    float m_maxHP;
    Vector3 m_knockbackVelocity{};
    static constexpr float kKnockbackDrag = 8.0f;

private:
    const std::vector<std::unique_ptr<Entity>>* m_obstacles = nullptr;
};
