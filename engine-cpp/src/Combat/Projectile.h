#pragma once
#include "raylib.h"

// Proyectil simple del Spitter: vuelo recto, sin gravedad ni rebote. Sin
// lógica propia -- CombatSystem::UpdateProjectiles lo mueve y comprueba el
// impacto contra el Player; Application lo dibuja y lo descarta cuando
// expira o impacta (ver Application::m_projectiles).
struct Projectile {
    Vector3 position{};
    Vector3 velocity{};
    float damage = 0.0f;
    float lifetime = 3.0f; // se autodestruye si no impacta nada a tiempo

    static constexpr float kRadius = 0.2f;
};
