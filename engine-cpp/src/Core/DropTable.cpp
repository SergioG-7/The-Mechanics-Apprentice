#include "DropTable.h"
#include "../IO/LevelLoader.h"
#include "../Entities/Gear.h"
#include "../Entities/HealthKit.h"
#include "../Entities/PowerUp.h"

namespace DropTable {

void RollEnemyDrop(LevelData& level, Vector3 position, bool endlessMode) {
    if (endlessMode) {
        level.gears.push_back(std::make_unique<Gear>(position));
    }

    // Una sola tirada de 1-100 recorrida por tramos, para que los porcentajes no se solapen.
    int roll = GetRandomValue(1, 100);
    int threshold = kOverclockChance;
    if (roll <= threshold) {
        level.powerUps.push_back(std::make_unique<PowerUp>(position, PowerUpType::Overclock));
        return;
    }

    threshold += kFrenzyChance;
    if (roll <= threshold) {
        level.powerUps.push_back(std::make_unique<PowerUp>(position, PowerUpType::Frenzy));
        return;
    }

    threshold += kShieldChance;
    if (roll <= threshold) {
        level.powerUps.push_back(std::make_unique<PowerUp>(position, PowerUpType::Shield));
        return;
    }

    threshold += kHealthKitChance;
    if (roll <= threshold) {
        level.healthKits.push_back(std::make_unique<HealthKit>(position));
    }
    // Resto (~88%): no suelta nada.
}

} // namespace DropTable
