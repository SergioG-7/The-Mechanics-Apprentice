#pragma once
#include "raylib.h"
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

// Carga los tres idiomas soportados (assets/lang/{es,en,jp}.json) de una vez
// al arrancar y mantiene los tres en memoria a la vez, aunque solo uno esté
// "activo" -- así LoadFonts puede construir el conjunto de codepoints para
// LoadFontEx con la UNIÓN de los tres idiomas, y cambiar de idioma en
// caliente nunca deja huecos en la fuente ya cargada.
//
// También es dueña de la fuente TTF (un atlas horneado POR CADA tamaño
// exacto que el juego dibuja en algún sitio, ver LoadFonts): vivía en
// Application, pero el conjunto de codepoints que necesita YA es de aquí
// (GetAllTextForCodepoints), así que hornear la fuente en el mismo sitio que
// calcula qué glifos necesita evita pasar ese conjunto de un lado a otro
// para un solo uso.
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

    // Libera todos los atlas explícitamente ANTES de CloseWindow() -- ver
    // Application::~Application() para el motivo (recursos de GPU liberados
    // contra un contexto ya cerrado si se dejara a la destrucción automática
    // de miembros). Idempotente: tras llamarla, el destructor la vuelve a
    // llamar y no hace nada (m_fonts ya queda vacío).
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

    // Devuelve el atlas horneado a EXACTAMENTE drawSize (redondeado al
    // entero más cercano) -- nunca una aproximación. Pixel-perfect real:
    // sin filtro bilineal en ningún punto de este archivo (raylib usa
    // nearest-neighbor por defecto en la textura de un Font recién
    // cargado), así que dibujar con CUALQUIER desajuste entre el tamaño
    // pedido y el tamaño horneado escala el atlas y se come píxeles de los
    // trazos finos -- el bug reportado ("letras rotas" en Noto Sans JP).
    // Todo tamaño que se le pase a DrawTextEx/MeasureTextEx en el proyecto
    // tiene que ser uno de los kFontSize* de abajo, nunca un literal suelto
    // -- así el horneado (LoadFonts) y el dibujado siempre citan la misma
    // constante y no pueden desincronizarse.
    const Font& GetFontForSize(float drawSize) const;

    // --- Tamaños exactos que dibuja el juego (auditoría de todos los
    // DrawTextEx/MeasureTextEx en MenuScreen.cpp/HudRenderer.cpp/
    // Application.cpp) -- cada uno hornea su propio atlas en LoadFonts.
    // Añadir un tamaño de texto nuevo en cualquier sitio del juego implica
    // añadir su constante aquí Y a kAllFontSizes (ver el .cpp); si no,
    // GetFontForSize cae a un atlas ajeno con un TraceLog de aviso en vez
    // de fallar en silencio, pero YA no será pixel-perfect.
    static constexpr float kFontSizeSliderLabel    = 20.0f; // MenuScreen::DrawVolumeSlider ("Música: 80%")
    static constexpr float kFontSizeControlsRow    = 22.0f; // MenuScreen::DrawControls: cabeceras y filas
    static constexpr float kFontSizeBody           = 24.0f; // MenuScreen::DrawButton (todos los botones) y DrawStats (filas)
    static constexpr float kFontSizeFps            = 26.0f; // Application::DrawGameplay: contador de FPS
    static constexpr float kFontSizeHud            = 28.0f; // HudRenderer::DrawHud: HP y engranajes
    static constexpr float kFontSizeOverlaySubtitle = 32.0f; // HudRenderer::DrawCenteredOverlay: subtítulo
    static constexpr float kFontSizeTitle          = 50.0f; // título de cada pantalla de menú (Principal, Opciones, Pausa, Controles, Estadísticas, Selector de nivel)
    static constexpr float kFontSizeOverlayTitle   = 90.0f; // HudRenderer::DrawCenteredOverlay: título grande (Ready/Game Over/Victory)

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

    // Un atlas por tamaño exacto (ver kAllFontSizes en el .cpp), indexado
    // por el tamaño entero horneado -- std::map, no unordered_map: son
    // como mucho un puñado de entradas, y poder recorrerlas en orden es lo
    // que necesita ClosestFont para el caso defensivo de un tamaño no
    // registrado.
    std::map<int, Font> m_fonts;
};
