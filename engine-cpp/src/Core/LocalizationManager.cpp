#include "LocalizationManager.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

using json = nlohmann::json;

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

    SetLanguage(initialLanguage);
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
