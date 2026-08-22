#pragma once
#include <algorithm>

// Temporizador simple que cuenta hacia atrás y está "activo" mientras quede tiempo.
// Se usa para cooldowns, parpadeos y efectos temporales (dash, hit-flash, screen shake...).
class CountdownTimer {
public:
    void Start(float duration) { m_remaining = duration; }
    void Tick(float dt) { m_remaining = std::max(0.0f, m_remaining - dt); }
    bool IsActive() const { return m_remaining > 0.0f; }
    float Remaining() const { return m_remaining; }

private:
    float m_remaining = 0.0f;
};
