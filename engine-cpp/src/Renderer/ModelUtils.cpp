#include "ModelUtils.h"
#include "rlgl.h"

namespace ModelUtils {

void UnloadOwnTextures(const Model& model) {
    for (int i = 0; i < model.materialCount; i++) {
        Texture2D albedo = model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture;
        if (albedo.id != rlGetTextureIdDefault()) UnloadTexture(albedo);
    }
}

void UnloadModelAndTextures(Model& model) {
    UnloadOwnTextures(model);
    UnloadModel(model);
}

void ApplyShaderToMaterials(Model& model, Shader shader) {
    for (int i = 0; i < model.materialCount; i++) {
        model.materials[i].shader = shader;
    }
}

void DrawModelWithOutline(const Model& model, Vector3 position, Vector3 rotationAxis,
                           float rotationAngleDegrees, Vector3 scale, Color tint) {
    constexpr float kOutlineScaleFactor = 1.03f;
    Vector3 outlineScale{ scale.x * kOutlineScaleFactor, scale.y * kOutlineScaleFactor, scale.z * kOutlineScaleFactor };

    rlSetCullFace(RL_CULL_FACE_FRONT);
    DrawModelEx(model, position, rotationAxis, rotationAngleDegrees, outlineScale, Color{ 0, 0, 0, tint.a });
    rlSetCullFace(RL_CULL_FACE_BACK);

    DrawModelEx(model, position, rotationAxis, rotationAngleDegrees, scale, tint);
}

} // namespace ModelUtils
