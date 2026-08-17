#pragma once
#include "Entity.h"

// Objeto de recolección que cura al Player al tocarlo -- a diferencia de
// Gear, no cuenta para ninguna condición de victoria/puntuación. Sin HP ni
// FSM, igual que Gear/Obstacle: hereda de Entity, no de Actor.
class HealthKit : public Entity {
public:
    explicit HealthKit(Vector3 position, float healAmount = 30.0f);

    void Update(float) override {}
    void Draw() const override;

    float GetHealAmount() const { return m_healAmount; }

private:
    float m_healAmount;
};
