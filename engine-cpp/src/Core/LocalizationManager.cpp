#include "LocalizationManager.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstdlib>

using json = nlohmann::json;

namespace {
// Todos los tamaños exactos que el juego dibuja en algún sitio -- ver la
// auditoría documentada junto a cada constante en LocalizationManager.h.
// Un solo array construido a partir de esas constantes (no literales
// repetidos aquí) para que hornear (este archivo) y dibujar (MenuScreen.cpp/
// HudRenderer.cpp/Application.cpp) no puedan desincronizarse por accidente.
constexpr float kAllFontSizes[] = {
    LocalizationManager::kFontSizeSliderLabel,
    LocalizationManager::kFontSizeControlsRow,
    LocalizationManager::kFontSizeBody,
    LocalizationManager::kFontSizeFps,
    LocalizationManager::kFontSizeHud,
    LocalizationManager::kFontSizeOverlaySubtitle,
    LocalizationManager::kFontSizeTitle,
    LocalizationManager::kFontSizeOverlayTitle,
};
} // namespace

LocalizationManager::~LocalizationManager() {
    UnloadFonts();
}

const std::vector<std::string>& LocalizationManager::SupportedLanguages() {
    // El orden es el orden de ciclado de CycleLanguage().
    static const std::vector<std::string> languages = { "es", "en", "jp" };
    return languages;
}

void LocalizationManager::LoadLanguageFile(const std::string& code) {
    std::string path = "assets/lang/" + code + ".json";
    std::ifstream file(path);
    if (!file.is_open()) {
        TraceLog(LOG_WARNING, "LocalizationManager: no se pudo abrir '%s'", path.c_str());
        return;
    }

    LanguageData data;
    try {
        json root;
        file >> root;
        for (const auto& [key, value] : root.items()) {
            data.entries.emplace(key, value.get<std::string>());
        }
    } catch (const json::exception& e) {
        TraceLog(LOG_WARNING, "LocalizationManager: '%s' mal formado (%s)", path.c_str(), e.what());
    }

    m_languages.emplace(code, std::move(data));
}

void LocalizationManager::LoadAll(const std::string& initialLanguage) {
    for (const std::string& code : SupportedLanguages()) {
        LoadLanguageFile(code);
    }

    // Unión de todo el texto de los tres idiomas: es la fuente de la que
    // Application saca los codepoints que necesita la fuente (ver
    // GetAllTextForCodepoints).
    m_allText.clear();
    for (const auto& [code, data] : m_languages) {
        for (const auto& [key, value] : data.entries) {
            m_allText += value;
            m_allText += ' ';
        }
    }

    // ASCII imprimible completo (32-126): dígitos, ':', '%' y demás
    // puntuación que el texto compone en tiempo de ejecución (TextFormat
    // del volumen, del contador de engranajes, etc.) no aparecen
    // literalmente en ninguna traducción, así que la unión de arriba no
    // los cubre. Sin esto, GetGlyphIndex no encuentra el codepoint y cae
    // a su fallback -- que a su vez solo es '?' si '?' está cargado; si
    // tampoco lo está, cae al glifo 0 (el primero insertado, orden no
    // determinista de unordered_map), que es el bug observado ("音量R
    // RRR": ':', '%' y los dígitos ausentes dibujando todos el mismo
    // glifo arbitrario).
    for (char c = 32; c <= 126; ++c) {
        m_allText += c;
    }

    SetLanguage(initialLanguage);
}

void LocalizationManager::LoadFonts() {
    int codepointCount = 0;
    int* codepoints = LoadCodepoints(m_allText.c_str(), &codepointCount);

    for (float size : kAllFontSizes) {
        int intSize = static_cast<int>(size);
        Font font;
        LoadFontAtSize(font, intSize, codepoints, codepointCount);
        m_fonts.emplace(intSize, font); // kAllFontSizes no tiene tamaños repetidos, así que emplace nunca pisa uno ya cargado
    }

    UnloadCodepoints(codepoints);
}

void LocalizationManager::LoadFontAtSize(Font& outFont, int size, int* codepoints, int codepointCount) {
    // MainFont.ttf (copiada de Tactical Soccer, Assets/Resources/MainFont.ttf).
    // Cobertura CJK verificada con fontTools antes de adoptarla (16732
    // glifos, cubre los 129 codepoints japoneses que usa este proyecto).
    //
    // El horneado a DOS tamaños (28/64px) de la sesión anterior seguía
    // perdiendo trazos finos porque solo era una aproximación: un botón a
    // 24px seguía usando el atlas de 28px (downscale ~14%), un título a
    // 90px seguía usando el de 64px (upscale ~40%) -- raylib no usa filtro
    // bilineal (textura de un Font recién cargado = nearest-neighbor por
    // defecto), así que CUALQUIER desajuste entre tamaño horneado y tamaño
    // dibujado salta píxeles de los trazos finos. La cura real no era mejor
    // fuente ni más tamaños intermedios, sino horneado 1:1: un atlas por
    // cada tamaño EXACTO que el juego dibuja (ver kAllFontSizes/LoadFonts),
    // así que GetFontForSize nunca tiene que escalar nada.
    outFont = LoadFontEx("assets/fonts/MainFont.ttf", size, codepoints, codepointCount);
    if (!IsFontValid(outFont)) {
        TraceLog(LOG_WARNING, "LocalizationManager: no se pudo cargar assets/fonts/MainFont.ttf a %dpx, usando la fuente por defecto", size);
        outFont = GetFontDefault();
    }
}

void LocalizationManager::UnloadFonts() {
    for (auto& [size, font] : m_fonts) {
        if (IsFontValid(font)) UnloadFont(font);
    }
    m_fonts.clear();
}

const Font& LocalizationManager::GetFontForSize(float drawSize) const {
    // Redondeo, no truncado: 27.6f tiene que caer en el atlas de 28, no en
    // el de 27 (que ni siquiera existe) ni perder medio píxel de precisión
    // contra el tamaño que de verdad se le pasa a DrawTextEx.
    int size = static_cast<int>(std::lround(drawSize));
    auto it = m_fonts.find(size);
    if (it != m_fonts.end()) return it->second;

    // No debería pasar nunca: todo DrawTextEx/MeasureTextEx del proyecto usa
    // una de las constantes kFontSize* de LocalizationManager.h, que son
    // justo las que arriba pueblan m_fonts. Si aun así llega un tamaño no
    // horneado (un DrawTextEx nuevo con un literal suelto en vez de una de
    // esas constantes), se cae al atlas más cercano en vez de crashear o
    // dejar el texto sin dibujar -- pero ya no será pixel-perfect, de ahí el
    // aviso.
    TraceLog(LOG_WARNING, "LocalizationManager: tamaño de fuente %dpx no horneado (falta registrarlo en kFontSize*), usando el atlas más cercano", size);
    return ClosestFont(size);
}

const Font& LocalizationManager::ClosestFont(int size) const {
    auto best = m_fonts.begin();
    int bestDiff = std::abs(best->first - size);
    for (auto it = m_fonts.begin(); it != m_fonts.end(); ++it) {
        int diff = std::abs(it->first - size);
        if (diff < bestDiff) {
            bestDiff = diff;
            best = it;
        }
    }
    return best->second;
}

void LocalizationManager::SetLanguage(const std::string& languageCode) {
    if (m_languages.find(languageCode) != m_languages.end()) {
        m_currentLanguage = languageCode;
    } else {
        TraceLog(LOG_WARNING, "LocalizationManager: idioma '%s' no soportado, usando 'es'", languageCode.c_str());
        m_currentLanguage = "es";
    }
}

void LocalizationManager::CycleLanguage() {
    const std::vector<std::string>& langs = SupportedLanguages();
    auto it = std::find(langs.begin(), langs.end(), m_currentLanguage);

    size_t nextIndex = 0;
    if (it != langs.end()) {
        size_t currentIndex = static_cast<size_t>(std::distance(langs.begin(), it));
        nextIndex = (currentIndex + 1) % langs.size();
    }
    m_currentLanguage = langs[nextIndex];
}

const char* LocalizationManager::GetText(const char* key) const {
    auto langIt = m_languages.find(m_currentLanguage);
    if (langIt != m_languages.end()) {
        auto entryIt = langIt->second.entries.find(key);
        if (entryIt != langIt->second.entries.end()) {
            return entryIt->second.c_str();
        }
    }

    TraceLog(LOG_WARNING, "LocalizationManager: clave '%s' no encontrada en '%s'", key, m_currentLanguage.c_str());
    return key;
}
