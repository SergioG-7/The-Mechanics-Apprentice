#pragma once
#include "../Core/LocalizationManager.h"

// Datos que necesita cualquier pantalla de UI para dibujar texto: el idioma activo y su fuente.
struct UiContext {
    const LocalizationManager& localization;
};
