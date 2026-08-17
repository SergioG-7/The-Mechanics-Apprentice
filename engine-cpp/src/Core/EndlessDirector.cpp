#include "EndlessDirector.h"
#include "../Entities/Spawner.h"

void EndlessDirector::Reset() {
    m_difficultyTimer = 0.0f;
    m_score = 0;
}

void EndlessDirector::Update(float dt, std::vector<Spawner>& spawners) {
    m_difficultyTimer += dt;
    if (m_difficultyTimer < kDifficultyInterval) return;

    m_difficultyTimer -= kDifficultyInterval;
    for (Spawner& spawner : spawners) {
        spawner.ScaleSpawnInterval(kDifficultyScaleFactor);
    }
}

void EndlessDirector::OnGearCollected() {
    ++m_score;
}
