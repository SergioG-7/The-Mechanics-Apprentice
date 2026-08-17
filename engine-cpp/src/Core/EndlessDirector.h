#pragma once
#include <vector>

class Spawner;

// Reglas propias del Modo Infinito, separadas de Application para que no
// cargue con esta lógica además de la orquestación de estados: escala la
// dificultad con el tiempo y lleva la puntuación (engranajes recogidos). El
// drop de un Gear al morir un enemigo lo decide Application (necesita
// conocer m_level.gears y el modo activo), EndlessDirector solo cuenta.
class EndlessDirector {
public:
    // Vuelve al estado inicial -- Application la llama al entrar en Modo
    // Infinito desde el menú, nunca al recargar el mismo intento.
    void Reset();

    // Cada kDifficultyInterval segundos, reduce el spawnInterval de TODOS
    // los spawners un kDifficultyScaleFactor (multiplicativo, no aditivo:
    // la dificultad se acelera con el tiempo en vez de crecer lineal).
    void Update(float dt, std::vector<Spawner>& spawners);

    void OnGearCollected();
    int GetScore() const { return m_score; }

private:
    float m_difficultyTimer = 0.0f;
    int m_score = 0;

    static constexpr float kDifficultyInterval = 15.0f;
    static constexpr float kDifficultyScaleFactor = 0.95f; // -5% cada intervalo
};
