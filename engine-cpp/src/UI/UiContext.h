#pragma once
#include "../Core/LocalizationManager.h"

// Dependencia que casi cualquier pantalla de UI necesita para dibujar texto:
// el gestor de idioma activo, que ahora también es dueño de la fuente (dos
// tamaños horneados, ver LocalizationManager::GetFontForSize) -- antes había
// un campo `font` aparte aquí, pero como la fuente correcta depende del
// tamaño de dibujado de cada texto, cada sitio pide la suya con
// ui.localization.GetFontForSize(size) en vez de recibir una sola fija.
struct UiContext {
    const LocalizationManager& localization;
};
