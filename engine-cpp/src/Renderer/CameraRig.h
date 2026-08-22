#pragma once
#include "raylib.h"
#include "../Core/CountdownTimer.h"

// Cámara isométrica que sigue al jugador, con efecto de screen shake.
class CameraRig {
public:
    CameraRig();

    // Recoloca la cámara sobre el objetivo y suma el shake activo.
    void FollowTarget(Vector3 target);

    void Tick(float dt) { m_shakeTimer.Tick(dt); }

    void AddShake(float duration, float intensity);

    // Corta el shake en seco, al cargar nivel o al pausar.
    void ClearShake();

    const Camera3D& Get() const { return m_camera; }

private:
    Camera3D m_camera{};
    CountdownTimer m_shakeTimer;
    float m_shakeIntensity = 0.0f;

    // Offset isométrico: 15 unidades arriba, 12 atrás.
    static constexpr float kHeight = 15.0f;
    static constexpr float kDistance = 12.0f;
};
