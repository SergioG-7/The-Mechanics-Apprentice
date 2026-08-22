#include "LocalizationManager.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <cmath>
#include <cstdlib>

using json = nlohmann::json;

namespace {
// Todos los tamaños de texto que dibuja el juego.
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

    // Junta todo el texto de los tres idiomas, para saber qué glifos necesita la fuente.
    m_allText.clear();
    for (const auto& [code, data] : m_languages) {
        for (const auto& [key, value] : data.entries) {
            m_allText += value;
            m_allText += ' ';
        }
    }

    // Añade también el ASCII imprimible completo: dígitos, ':', '%' y demás
    // símbolos que el texto compone en tiempo de ejecución y no aparecen en ninguna traducción.
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
        m_fonts.emplace(intSize, font);
    }

    UnloadCodepoints(codepoints);
}

void LocalizationManager::LoadFontAtSize(Font& outFont, int size, int* codepoints, int codepointCount) {
    // Hornea un atlas al tamaño exacto pedido, para que el texto se vea nítido a cualquier tamaño.
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
    int size = static_cast<int>(std::lround(drawSize));
    auto it = m_fonts.find(size);
    if (it != m_fonts.end()) return it->second;

    TraceLog(LOG_WARNING, "LocalizationManager: tamaño de fuente %dpx no horneado, usando el atlas más cercano", size);
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
