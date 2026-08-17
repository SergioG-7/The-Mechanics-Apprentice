#include "MenuScreen.h"

namespace {

constexpr float kButtonWidth = 300.0f;
constexpr float kButtonHeight = 60.0f;
constexpr float kButtonGap = 20.0f;

// Botón índice `index` de `count`, apilados y centrados verticalmente en
// pantalla. GetScreenWidth/Height en vez de constantes: si algún día cambia
// el tamaño de ventana en main.cpp, el menú se recalcula solo.
Rectangle StackedButton(int index, int count) {
    float screenW = static_cast<float>(GetScreenWidth());
    float screenH = static_cast<float>(GetScreenHeight());
    float totalHeight = count * kButtonHeight + (count - 1) * kButtonGap;
    float startY = (screenH - totalHeight) / 2.0f;
    float x = (screenW - kButtonWidth) / 2.0f;
    float y = startY + index * (kButtonHeight + kButtonGap);
    return Rectangle{ x, y, kButtonWidth, kButtonHeight };
}

} // namespace

Rectangle MenuScreen::StoryButtonBounds()   { return StackedButton(0, 4); }
Rectangle MenuScreen::EndlessButtonBounds() { return StackedButton(1, 4); }
Rectangle MenuScreen::OptionsButtonBounds() { return StackedButton(2, 4); }
Rectangle MenuScreen::QuitButtonBounds()    { return StackedButton(3, 4); }

// Opciones solo tiene el slider y "Volver": reutiliza el mismo apilado con
// count = 2, ocupando la posición del botón inferior.
Rectangle MenuScreen::BackButtonBounds() { return StackedButton(1, 2); }

Rectangle MenuScreen::VolumeSliderBounds() {
    Rectangle back = BackButtonBounds();
    return Rectangle{ back.x, back.y - 100.0f, back.width, 30.0f };
}

bool MenuScreen::IsButtonClicked(Rectangle bounds) {
    return IsMouseButtonPressed(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), bounds);
}

void MenuScreen::DrawButton(Rectangle bounds, const char* label) {
    bool hovered = CheckCollisionPointRec(GetMousePosition(), bounds);
    DrawRectangleRec(bounds, hovered ? Color{ 80, 80, 95, 255 } : Color{ 50, 50, 60, 255 });
    DrawRectangleLinesEx(bounds, 2.0f, RAYWHITE);

    constexpr int textSize = 24;
    int textWidth = MeasureText(label, textSize);
    DrawText(label,
             static_cast<int>(bounds.x + (bounds.width - static_cast<float>(textWidth)) / 2.0f),
             static_cast<int>(bounds.y + (bounds.height - static_cast<float>(textSize)) / 2.0f),
             textSize, RAYWHITE);
}

MenuAction MenuScreen::UpdateMainMenu() const {
    if (IsButtonClicked(StoryButtonBounds()))   return MenuAction::StartStory;
    if (IsButtonClicked(EndlessButtonBounds())) return MenuAction::StartEndless;
    if (IsButtonClicked(OptionsButtonBounds())) return MenuAction::OpenOptions;
    if (IsButtonClicked(QuitButtonBounds()))    return MenuAction::Quit;
    return MenuAction::None;
}

void MenuScreen::DrawMainMenu() const {
    const char* title = "LEVEL-5 PORTFOLIO";
    constexpr int titleSize = 50;
    DrawText(title, (GetScreenWidth() - MeasureText(title, titleSize)) / 2, 100, titleSize, RAYWHITE);

    DrawButton(StoryButtonBounds(), "Modo Historia");
    DrawButton(EndlessButtonBounds(), "Modo Infinito");
    DrawButton(OptionsButtonBounds(), "Opciones");
    DrawButton(QuitButtonBounds(), "Salir");
}

MenuAction MenuScreen::UpdateOptions() const {
    Rectangle slider = VolumeSliderBounds();
    if (IsMouseButtonDown(MOUSE_BUTTON_LEFT) && CheckCollisionPointRec(GetMousePosition(), slider)) {
        float ratio = (GetMousePosition().x - slider.x) / slider.width;
        if (ratio < 0.0f) ratio = 0.0f;
        if (ratio > 1.0f) ratio = 1.0f;
        SetMasterVolume(ratio);
    }

    if (IsButtonClicked(BackButtonBounds())) return MenuAction::BackToMainMenu;
    return MenuAction::None;
}

void MenuScreen::DrawOptions() const {
    const char* title = "OPCIONES";
    constexpr int titleSize = 50;
    DrawText(title, (GetScreenWidth() - MeasureText(title, titleSize)) / 2, 100, titleSize, RAYWHITE);

    float volume = GetMasterVolume();
    Rectangle slider = VolumeSliderBounds();

    const char* volumeLabel = TextFormat("Volumen: %d%%", static_cast<int>(volume * 100.0f));
    DrawText(volumeLabel, (GetScreenWidth() - MeasureText(volumeLabel, 20)) / 2,
             static_cast<int>(slider.y) - 30, 20, RAYWHITE);

    DrawRectangleRec(slider, Color{ 40, 40, 48, 255 });
    DrawRectangleRec(Rectangle{ slider.x, slider.y, slider.width * volume, slider.height }, SKYBLUE);
    DrawRectangleLinesEx(slider, 2.0f, RAYWHITE);

    DrawButton(BackButtonBounds(), "Volver");
}
