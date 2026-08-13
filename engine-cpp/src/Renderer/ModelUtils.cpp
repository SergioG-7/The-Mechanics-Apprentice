#include "ModelUtils.h"
#include "rlgl.h"

namespace ModelUtils {

void UnloadOwnTextures(const Model& model) {
    for (int i = 0; i < model.materialCount; i++) {
        Texture2D albedo = model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture;
        if (albedo.id != rlGetTextureIdDefault()) UnloadTexture(albedo);
    }
}

} // namespace ModelUtils
