#pragma once
#include "raylib.h"

namespace ModelUtils {

// Libera las texturas propias de un modelo (raylib no lo hace por defecto).
void UnloadOwnTextures(const Model& model);

// Libera un modelo junto con sus texturas.
void UnloadModelAndTextures(Model& model);

// Asigna un shader a todos los materiales de un modelo.
void ApplyShaderToMaterials(Model& model, Shader shader);

// Dibuja un modelo con un contorno negro estilo anime alrededor.
void DrawModelWithOutline(const Model& model, Vector3 position, Vector3 rotationAxis,
                           float rotationAngleDegrees, Vector3 scale, Color tint);

} // namespace ModelUtils
