#pragma once
#include <algorithm>

// Patrón repetido en Player, Enemy y Application: un float que cuenta hacia
// atrás desde un valor y se considera "activo" mientras sea > 0 (hit-flash,
// dash cooldown, screen shake, hit-stop...). No cubre los timers de la FSM
// que cuentan HACIA ARRIBA hasta una duración fija (m_attackTimer,
// m_hurtTimer...) -- esos tienen semántica distinta (disparan una
// transición de estado al llegar al tope) y forzarlos aquí no aclararía nada.
class CountdownTimer {
public:
    void Start(float duration) { m_remaining = duration; }
    void Tick(float dt) { m_remaining = std::max(0.0f, m_remaining - dt); }
    bool IsActive() const { return m_remaining > 0.0f; }
    float Remaining() const { return m_remaining; }

private:
    float m_remaining = 0.0f;
};
