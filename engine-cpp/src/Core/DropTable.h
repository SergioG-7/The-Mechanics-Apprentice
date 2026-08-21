#pragma once
#include "raylib.h"

struct LevelData;

// Qué suelta un enemigo al morir. Sale de Application porque es una decisión
// de DISEÑO (probabilidades de botín), no de orquestación de estados: aquí
// se puede leer y retocar el balance sin abrir el bucle de partida, y
// Application deja de tener una tabla de porcentajes entre medias del
// código que gestiona pausas y pantallas.
namespace DropTable {

// Empuja el botín directamente a las listas del nivel. endlessMode cambia una
// sola cosa: además del sorteo, se garantiza un engranaje, porque en Infinito
// el engranaje ES la puntuación del modo y no un extra.
void RollEnemyDrop(LevelData& level, Vector3 position, bool endlessMode);

// Probabilidades sobre 100, en orden de rareza. Suman ~12%: bajado del 35%
// del primer playtest, donde caían tantos que los efectos temporales estaban
// casi siempre activos y dejaban de sentirse como un golpe de suerte.
inline constexpr int kOverclockChance = 4;
inline constexpr int kFrenzyChance = 4;
inline constexpr int kShieldChance = 2;
inline constexpr int kHealthKitChance = 2;

} // namespace DropTable
