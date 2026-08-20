#pragma once
#include "raylib.h"
#include <string>
#include <unordered_map>
#include <vector>

// Carga los tres idiomas soportados (assets/lang/{es,en,jp}.json) de una vez
// al arrancar y mantiene los tres en memoria a la vez, aunque solo uno esté
// "activo" -- así LoadFonts puede construir el conjunto de codepoints para
// LoadFontEx con la UNIÓN de los tres idiomas, y cambiar de idioma en
// caliente nunca deja huecos en la fuente ya cargada.
//
// También es dueña de la fuente TTF (dos tamaños horneados, ver LoadFonts):
// vivía en Application, pero el conjunto de codepoints que necesita YA es de
// aquí (GetAllTextForCodepoints), así que hornear la fuente en el mismo
// sitio que calcula qué glifos necesita evita pasar ese conjunto de un lado
// a otro para un solo uso.
class LocalizationManager {
public:
    ~LocalizationManager();

    void LoadAll(const std::string& initialLanguage);

    // Debe llamarse DESPUÉS de LoadAll (necesita GetAllTextForCodepoints) y
    // con la ventana ya creada (InitWindow, para el contexto GL) -- por eso
    // es un método aparte y no algo que LoadAll haga por su cuenta:
    // LocalizationManager es un miembro por VALOR de Application, así que se
    // construye antes de que InitWindow/InitAudioDevice lleguen a correr.
    void LoadFonts();

    // Libera las dos fuentes explícitamente ANTES de CloseWindow() -- ver
    // Application::~Application() para el motivo (recursos de GPU liberados
    // contra un contexto ya cerrado si se dejara a la destrucción automática
    // de miembros). Idempotente: tras llamarla, el destructor la vuelve a
    // llamar y no hace nada (las fuentes ya quedan inválidas).
    void UnloadFonts();

    void SetLanguage(const std::string& languageCode);
    void CycleLanguage(); // es -> en -> jp -> es
    const std::string& GetCurrentLanguage() const { return m_currentLanguage; }

    // key debe ser un literal de C (vida estática): si la clave no existe,
    // se devuelve el propio puntero de entrada como fallback visible en
    // pantalla en vez de un texto vacío o un crash -- eso solo es seguro
    // porque key nunca es un std::string temporal.
    const char* GetText(const char* key) const;

    // Concatenación de TODO el texto de los tres idiomas, construida una
    // vez en LoadAll. LoadFonts se la pasa a LoadCodepoints() para saber
    // qué glifos necesita la fuente.
    const std::string& GetAllTextForCodepoints() const { return m_allText; }

    // La de los dos tamaños horneados (kSmallFontSize/kLargeFontSize) más
    // cercana a drawSize, con preferencia por NO reducir cuando sea posible:
    // sin filtro bilineal (ver LoadFontAtSize), reducir un atlas grande
    // salta píxeles y puede romper trazos finos ("letras rotas"); agrandar
    // uno pequeño solo lo vuelve más bloque, sin perder continuidad del
    // trazo. Por debajo del punto medio entre los dos tamaños se usa la
    // pequeña; en o por encima, la grande.
    const Font& GetFontForSize(float drawSize) const;

private:
    struct LanguageData {
        std::unordered_map<std::string, std::string> entries;
    };

    void LoadLanguageFile(const std::string& code);
    static const std::vector<std::string>& SupportedLanguages();
    static void LoadFontAtSize(Font& outFont, int size, int* codepoints, int codepointCount);

    std::unordered_map<std::string, LanguageData> m_languages; // clave: "es"/"en"/"jp"
    std::string m_currentLanguage = "es";
    std::string m_allText;

    static constexpr int kSmallFontSize = 32;   // UI/HUD: botones, tablas, HUD, subtítulos de overlay
    static constexpr int kLargeFontSize = 64;   // Títulos de pantalla y overlays de fin de partida
    static constexpr float kFontSizeThreshold = (kSmallFontSize + kLargeFontSize) / 2.0f;
    Font m_smallFont{};
    Font m_largeFont{};
};
