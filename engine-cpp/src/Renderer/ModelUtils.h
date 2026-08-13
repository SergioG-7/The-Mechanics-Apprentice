#pragma once
#include "raylib.h"

namespace ModelUtils {

// raylib 6.0 cambió UnloadModel: ya NO libera las texturas de los
// materiales (para no romper texturas compartidas entre modelos), solo la
// memoria de sus arrays -- ver rmodels.c. Si el modelo trae una textura
// real (como los atlas de Kenney), hay que liberarla a mano o se filtra.
// Se salta la textura por defecto de raylib, que es compartida y global.
void UnloadOwnTextures(const Model& model);

} // namespace ModelUtils
