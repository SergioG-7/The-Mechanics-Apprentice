#pragma once
#include <string>

// Datos persistentes entre sesiones, guardados en save_data.json.
struct SaveData {
    int highScore = 0;
    int zombiesKilled = 0;
    int barrelsExploded = 0;
    int healthKitsUsed = 0;
    int maxLevelUnlocked = 1;
    std::string currentLanguage = "es";
    float bgmVolume = 0.5f; // 0 a 1
    float sfxVolume = 1.0f;
};

// Carga y guarda los datos de partida en disco.
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
