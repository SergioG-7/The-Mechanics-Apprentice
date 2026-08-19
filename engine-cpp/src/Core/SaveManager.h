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
