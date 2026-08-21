#pragma once
#include "raylib.h"
#include "../Core/CountdownTimer.h"

// Cámara isométrica que sigue al jugador, más el screen shake. Vivía suelta
// en Application (Camera3D + timer + intensidad + el bloque de seguimiento
// dentro de UpdateGameplay), que ya cargaba con orquestación de estados,
// audio, HUD y reglas de partida. Aquí es una sola responsabilidad con su
// propio estado: nadie de fuera puede escribir la posición de la cámara y
// pelearse con el offset isométrico.
class CameraRig {
public:
    CameraRig();

    // Recoloca la cámara sobre el objetivo y suma el shake que quede activo.
    // Una sola llamada por frame; el shake se descuenta con Tick(), aparte,
    // para que siga corriendo aunque la partida no se esté actualizando.
    void FollowTarget(Vector3 target);

    void Tick(float dt) { m_shakeTimer.Tick(dt); }

    void AddShake(float duration, float intensity);

    // Corta el shake en seco. La llama Application al cargar nivel o al
    // pausar: un temblor a medias no debe sobrevivir a un cambio de pantalla.
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
