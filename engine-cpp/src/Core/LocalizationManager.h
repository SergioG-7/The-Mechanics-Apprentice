#pragma once
#include "raylib.h"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// Carga los idiomas soportados y gestiona el texto y la fuente del juego.
class LocalizationManager {
public:
    ~LocalizationManager();

    // Carga los tres idiomas y arranca en el indicado.
    void LoadAll(const std::string& initialLanguage);

    // Hornea los atlas de fuente para todos los tamaños de texto del juego.
    void LoadFonts();

    // Libera los atlas de fuente antes de cerrar la ventana.
    void UnloadFonts();

    void SetLanguage(const std::string& languageCode);
    void CycleLanguage(); // es -> en -> jp -> es
    const std::string& GetCurrentLanguage() const { return m_currentLanguage; }

    // Traduce una clave al idioma activo. Si no existe, devuelve la propia clave.
    const char* GetText(const char* key) const;

    // Todo el texto de los tres idiomas junto, usado para saber qué glifos necesita la fuente.
    const std::string& GetAllTextForCodepoints() const { return m_allText; }

    // Devuelve el atlas horneado al tamaño exacto pedido.
    const Font& GetFontForSize(float drawSize) const;

    // Tamaños de texto usados en el juego; cada uno tiene su propio atlas horneado.
    static constexpr float kFontSizeSliderLabel    = 20.0f;
    static constexpr float kFontSizeControlsRow    = 22.0f;
    static constexpr float kFontSizeBody           = 24.0f;
    static constexpr float kFontSizeFps            = 26.0f;
    static constexpr float kFontSizeHud            = 28.0f;
    static constexpr float kFontSizeOverlaySubtitle = 32.0f;
    static constexpr float kFontSizeTitle          = 50.0f;
    static constexpr float kFontSizeOverlayTitle   = 90.0f;

private:
    struct LanguageData {
        std::unordered_map<std::string, std::string> entries;
    };

    void LoadLanguageFile(const std::string& code);
    static const std::vector<std::string>& SupportedLanguages();
    static void LoadFontAtSize(Font& outFont, int size, int* codepoints, int codepointCount);
    const Font& ClosestFont(int size) const;

    std::unordered_map<std::string, LanguageData> m_languages; // clave: "es"/"en"/"jp"
    std::string m_currentLanguage = "es";
    std::string m_allText;

    // Un atlas por cada tamaño de texto usado en el juego.
    std::map<int, Font> m_fonts;
};
