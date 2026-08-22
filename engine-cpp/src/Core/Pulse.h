#pragma once
#include <cmath>

// Funciones de animación cíclica compartidas: parpadeos, auras, flotación.
// Reciben siempre el tiempo por parámetro, así no dependen del reloj de raylib.
namespace Pulse {

// Parpadeo clásico: true durante la primera mitad de cada periodo.
inline bool Blink(float elapsed, float period) {
    constexpr float kMinPeriod = 0.02f; // evita dividir por un periodo casi cero
    if (period < kMinPeriod) period = kMinPeriod;
    return fmodf(elapsed, period) < (period * 0.5f);
}

// Parpadeo que se acelera con el progreso (0..1), de startPeriod a endPeriod.
inline bool AcceleratingBlink(float elapsed, float progress, float startPeriod, float endPeriod) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    return Blink(elapsed, startPeriod + (endPeriod - startPeriod) * progress);
}

// Onda suave entre 0 y 1, para auras o halos que laten.
inline float Wave01(float elapsed, float speed) {
    return 0.5f + 0.5f * sinf(elapsed * speed);
}

// Progreso de una duración, recortado siempre entre 0 y 1.
inline float Progress01(float elapsed, float duration) {
    if (duration <= 0.0f) return 1.0f;
    float progress = elapsed / duration;
    if (progress < 0.0f) return 0.0f;
    if (progress > 1.0f) return 1.0f;
    return progress;
}

} // namespace Pulse
