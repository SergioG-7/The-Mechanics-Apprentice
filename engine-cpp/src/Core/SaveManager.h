#pragma once
#include <string>

// Datos persistentes entre sesiones -- todo lo que SaveManager lee/escribe
// en save_data.json vive aquí, en un solo sitio.
struct SaveData {
    int highScore = 0;
    int zombiesKilled = 0;
    int barrelsExploded = 0;
    int healthKitsUsed = 0;
    int maxLevelUnlocked = 1;
    std::string currentLanguage = "es";

    // 0.0 (silencio) - 1.0 (volumen real de SetMusicVolume/SetSoundVolume,
    // sin ningún multiplicador oculto de por medio -- ver
    // Player::RefreshSfxVolume). bgmVolume por defecto en 0.5, no 1.0: la
    // música de fondo suena de fondo, no compite con los SFX. sfxVolume en
    // 1.0 porque no hay razón para arrancar los SFX atenuados.
    float bgmVolume = 0.5f;
    float sfxVolume = 1.0f;
};

// Lee save_data.json al construirse (o deja SaveData en sus valores por
// defecto si no existe todavía, p.ej. primera vez que se ejecuta el juego).
// Application actualiza Data() directamente según ocurren las cosas
// (matar un enemigo, usar un botiquín...) y llama a Save() en los puntos
// que importan (morir, completar nivel, cambiar idioma) -- SaveManager no
// decide CUÁNDO guardar, solo CÓMO.
class SaveManager {
public:
    SaveManager();

    void Save() const;

    SaveData& Data() { return m_data; }
    const SaveData& Data() const { return m_data; }

private:
    void Load();

    SaveData m_data;
    static constexpr const char* kSavePath = "save_data.json";
};
