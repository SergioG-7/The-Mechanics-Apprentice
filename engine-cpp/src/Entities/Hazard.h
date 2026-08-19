#pragma once
#include "Entity.h"

// Trampa de suelo: NO bloquea el paso (no vive en LevelData::obstacles, así
// que Entity::TryMove nunca la ve) pero daña por tick a quien esté dentro de
// su AABB mientras se queda. Vive en su propia lista (LevelData::hazards)
// porque, a diferencia de Obstacle/Cylinder, necesita Update() real (el
// timer de tick) y CombatSystem necesita leer GetDamagePerTick()/ConsumeTick()
// -- ninguno de los dos existe en Entity, igual que ExplosiveBarrel necesita
// su propia lista en vez de vivir como Actor genérico.
class Hazard : public Entity {
public:
    Hazard(Vector3 position, Vector3 size, float damagePerTick);

    void Update(float dt) override;
    void Draw() const override;

    float GetDamagePerTick() const { return m_damagePerTick; }

    // true como mucho una vez cada kTickInterval segundos -- CombatSystem la
    // llama una vez por hazard y frame; si toca tick, aplica daño a todo lo
    // que se solape con GetBoundingBox() en ese instante.
    bool ConsumeTick();

    static constexpr float kTickInterval = 0.8f;

private:
    float m_damagePerTick;
    float m_tickTimer = 0.0f;
};
