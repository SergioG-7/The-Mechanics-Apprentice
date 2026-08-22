#pragma once
#include "raylib.h"

struct LevelData;

// Qué suelta un enemigo al morir.
namespace DropTable {

// Sortea y añade el botín de un enemigo muerto a las listas del nivel.
// En Modo Infinito, además del sorteo, siempre suelta un engranaje.
void RollEnemyDrop(LevelData& level, Vector3 position, bool endlessMode);

// Probabilidades de drop sobre 100.
inline constexpr int kOverclockChance = 4;
inline constexpr int kFrenzyChance = 4;
inline constexpr int kShieldChance = 2;
inline constexpr int kHealthKitChance = 2;

} // namespace DropTable
