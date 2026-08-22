#pragma once
#include <vector>

class Spawner;

// Reglas del Modo Infinito: escala la dificultad con el tiempo y lleva la puntuación.
class EndlessDirector {
public:
    // Vuelve al estado inicial, al entrar en Modo Infinito.
    void Reset();

    // Acelera el ritmo de generación de todos los spawners con el paso del tiempo.
    void Update(float dt, std::vector<Spawner>& spawners);

    void OnGearCollected();
    int GetScore() const { return m_score; }

private:
    float m_difficultyTimer = 0.0f;
    int m_score = 0;

    static constexpr float kDifficultyInterval = 15.0f;
    static constexpr float kDifficultyScaleFactor = 0.95f; // -5% cada intervalo
};
