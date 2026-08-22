#pragma once
#include "Entity.h"

// Trampa de suelo: no bloquea el paso, pero daña por ticks a quien se quede dentro.
class Hazard : public Entity {
public:
    Hazard(Vector3 position, Vector3 size, float damagePerTick);

    void Update(float dt) override;
    void Draw() const override;

    float GetDamagePerTick() const { return m_damagePerTick; }

    // Devuelve true cuando toca aplicar el siguiente tick de daño.
    bool ConsumeTick();

    static constexpr float kTickInterval = 0.8f;

private:
    float m_damagePerTick;
    float m_tickTimer = 0.0f;
};
