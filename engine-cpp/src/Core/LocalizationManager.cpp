#include "LocalizationManager.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

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

    LoadFontAtSize(m_smallFont, kSmallFontSize, codepoints, codepointCount);
    LoadFontAtSize(m_largeFont, kLargeFontSize, codepoints, codepointCount);

    UnloadCodepoints(codepoints);
}

void LocalizationManager::LoadFontAtSize(Font& outFont, int size, int* codepoints, int codepointCount) {
    // MainFont.ttf (copiada de Tactical Soccer, Assets/Resources/MainFont.ttf):
    // sustituye a la font.ttf original tras el playtest 6, que reportó trazos
    // finos perdidos ("letras rotas") incluso en los tamaños horneados exactos
    // -- el problema no era solo de escalado (ya resuelto por el propio
    // horneado a dos tamaños), sino del diseño de glifo de la fuente anterior
    // a tamaños pequeños. Cobertura CJK verificada con fontTools antes de
    // adoptarla (16732 glifos, cubre los 129 codepoints japoneses que usa
    // este proyecto).
    outFont = LoadFontEx("assets/fonts/MainFont.ttf", size, codepoints, codepointCount);
    if (!IsFontValid(outFont)) {
        TraceLog(LOG_WARNING, "LocalizationManager: no se pudo cargar assets/fonts/MainFont.ttf a %dpx, usando la fuente por defecto", size);
        outFont = GetFontDefault();
    }
}

void LocalizationManager::UnloadFonts() {
    if (IsFontValid(m_smallFont)) { UnloadFont(m_smallFont); m_smallFont = Font{}; }
    if (IsFontValid(m_largeFont)) { UnloadFont(m_largeFont); m_largeFont = Font{}; }
}

const Font& LocalizationManager::GetFontForSize(float drawSize) const {
    return (drawSize < kFontSizeThreshold) ? m_smallFont : m_largeFont;
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
