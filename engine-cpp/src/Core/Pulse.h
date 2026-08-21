#pragma once
#include <cmath>

// Interpolaciones cíclicas de presentación, compartidas por todo lo que
// "late" en pantalla. Antes cada entidad reescribía la misma fórmula con sus
// propias constantes mágicas: el parpadeo del cadáver y el del Kamikaze en
// Enemy, el aviso de ElectricTile, el aura del Buffer, el flotar del PowerUp
// y el anillo del Spawner aleatorio. Son funciones puras -- ninguna llama a
// GetTime() por su cuenta, el tiempo entra siempre por parámetro, así que se
// pueden razonar (y probar) sin depender del reloj de raylib.
namespace Pulse {

// Onda cuadrada: true durante la primera mitad de cada periodo. El parpadeo
// clásico "encendido/apagado" a ritmo constante.
inline bool Blink(float elapsed, float period) {
    // Suelo del periodo: un fmodf contra algo casi cero degenera en un
    // parpadeo por frame (o en una división absurda) en vez de en un
    // destello legible. Ya se protegía a mano en dos sitios distintos.
    constexpr float kMinPeriod = 0.02f;
    if (period < kMinPeriod) period = kMinPeriod;
    return fmodf(elapsed, period) < (period * 0.5f);
}

// Igual que Blink pero con el periodo interpolado linealmente de startPeriod
// a endPeriod según progress (0..1): el parpadeo se acelera a medida que se
// acerca el evento, y el propio ritmo hace de barra de progreso. Lo usan el
// Kamikaze antes de detonar y la baldosa eléctrica antes de descargar.
inline bool AcceleratingBlink(float elapsed, float progress, float startPeriod, float endPeriod) {
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    return Blink(elapsed, startPeriod + (endPeriod - startPeriod) * progress);
}

// Seno normalizado a 0..1 -- el "respirar" suave de un aura o un halo.
inline float Wave01(float elapsed, float speed) {
    return 0.5f + 0.5f * sinf(elapsed * speed);
}

// Fracción recorrida de una duración, recortada a 0..1. Evita el
// `t / duration` suelto que puede salirse de rango en un frame largo.
inline float Progress01(float elapsed, float duration) {
    if (duration <= 0.0f) return 1.0f;
    float progress = elapsed / duration;
    if (progress < 0.0f) return 0.0f;
    if (progress > 1.0f) return 1.0f;
    return progress;
}

} // namespace Pulse
