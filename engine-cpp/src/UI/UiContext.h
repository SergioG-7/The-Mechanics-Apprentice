#pragma once
#include "raylib.h"
#include "../Core/LocalizationManager.h"

// Dependencias que casi cualquier pantalla de UI necesita para dibujar
// texto: la fuente cargada (con los glifos de los tres idiomas, incluido
// japonés) y el gestor de idioma activo. Un solo parámetro compuesto en vez
// de dos sueltos porque casi todos los métodos de MenuScreen los necesitan
// juntos -- ver Application::BuildFontCodepoints / m_font.
struct UiContext {
    const Font& font;
    const LocalizationManager& localization;
};
