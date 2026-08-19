#pragma once
#include "raylib.h"

namespace ModelUtils {

// raylib 6.0 cambió UnloadModel: ya NO libera las texturas de los
// materiales (para no romper texturas compartidas entre modelos), solo la
// memoria de sus arrays -- ver rmodels.c. Si el modelo trae una textura
// real (como los atlas de Kenney), hay que liberarla a mano o se filtra.
// Se salta la textura por defecto de raylib, que es compartida y global.
void UnloadOwnTextures(const Model& model);

// UnloadOwnTextures + UnloadModel en un solo sitio: Player (cuerpo y arma) y
// Enemy repetían las dos llamadas juntas en cada destructor.
void UnloadModelAndTextures(Model& model);

// Asigna shader a todos los materiales de un modelo -- Player (cuerpo y
// arma) y Enemy::SetShader hacían el mismo bucle por separado.
void ApplyShaderToMaterials(Model& model, Shader shader);

// Silueta estilo anime ("inverted hull"): dibuja el modelo un ~3% más
// grande con el culling invertido (solo caras traseras, que sobresalen
// justo alrededor de la silueta normal) en negro puro con el mismo alpha
// que tint, y luego el modelo normal encima. Player::Draw y Enemy::Draw
// tenían la misma secuencia rlSetCullFace/DrawModelEx duplicada.
void DrawModelWithOutline(const Model& model, Vector3 position, Vector3 rotationAxis,
                           float rotationAngleDegrees, Vector3 scale, Color tint);

} // namespace ModelUtils
