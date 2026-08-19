#pragma once
#include <string>
#include <unordered_map>
#include <vector>

// Carga los tres idiomas soportados (assets/lang/{es,en,jp}.json) de una vez
// al arrancar y mantiene los tres en memoria a la vez, aunque solo uno esté
// "activo" -- así Application puede construir el conjunto de codepoints para
// LoadFontEx con la UNIÓN de los tres idiomas, y cambiar de idioma en
// caliente nunca deja huecos en la fuente ya cargada.
class LocalizationManager {
public:
    void LoadAll(const std::string& initialLanguage);

    void SetLanguage(const std::string& languageCode);
    void CycleLanguage(); // es -> en -> jp -> es
    const std::string& GetCurrentLanguage() const { return m_currentLanguage; }

    // key debe ser un literal de C (vida estática): si la clave no existe,
    // se devuelve el propio puntero de entrada como fallback visible en
    // pantalla en vez de un texto vacío o un crash -- eso solo es seguro
    // porque key nunca es un std::string temporal.
    const char* GetText(const char* key) const;

    // Concatenación de TODO el texto de los tres idiomas, construida una
    // vez en LoadAll. Application se la pasa a LoadCodepoints() para saber
    // qué glifos necesita la fuente.
    const std::string& GetAllTextForCodepoints() const { return m_allText; }

private:
    struct LanguageData {
        std::unordered_map<std::string, std::string> entries;
    };

    void LoadLanguageFile(const std::string& code);
    static const std::vector<std::string>& SupportedLanguages();

    std::unordered_map<std::string, LanguageData> m_languages; // clave: "es"/"en"/"jp"
    std::string m_currentLanguage = "es";
    std::string m_allText;
};
