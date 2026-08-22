#pragma once
#include "Entity.h"

// Botiquín que cura al jugador al recogerlo.
class HealthKit : public Entity {
public:
    explicit HealthKit(Vector3 position, float healAmount = 30.0f);

    void Update(float) override {}
    void Draw() const override;

    float GetHealAmount() const { return m_healAmount; }

private:
    float m_healAmount;
};
