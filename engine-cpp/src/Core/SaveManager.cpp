#include "SaveManager.h"
#include "raylib.h"
#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

SaveManager::SaveManager() {
    Load();
}

void SaveManager::Load() {
    std::ifstream file(kSavePath);
    if (!file.is_open()) {
        // Primera ejecución, o el archivo se borró a mano: se queda con los
        // valores por defecto de SaveData, no es un error.
        return;
    }

    try {
        json root;
        file >> root;
        m_data.highScore = root.value("HighScore", m_data.highScore);
        m_data.zombiesKilled = root.value("ZombiesKilled", m_data.zombiesKilled);
        m_data.barrelsExploded = root.value("BarrelsExploded", m_data.barrelsExploded);
        m_data.healthKitsUsed = root.value("HealthKitsUsed", m_data.healthKitsUsed);
        m_data.maxLevelUnlocked = root.value("MaxLevelUnlocked", m_data.maxLevelUnlocked);
        m_data.currentLanguage = root.value("CurrentLanguage", m_data.currentLanguage);
        m_data.bgmVolume = root.value("BGMVolume", m_data.bgmVolume);
        m_data.sfxVolume = root.value("SFXVolume", m_data.sfxVolume);
    } catch (const json::exception& e) {
        TraceLog(LOG_WARNING, "SaveManager: '%s' mal formado (%s), usando valores por defecto", kSavePath, e.what());
        m_data = SaveData{};
    }
}

void SaveManager::Save() const {
    json root;
    root["HighScore"] = m_data.highScore;
    root["ZombiesKilled"] = m_data.zombiesKilled;
    root["BarrelsExploded"] = m_data.barrelsExploded;
    root["HealthKitsUsed"] = m_data.healthKitsUsed;
    root["MaxLevelUnlocked"] = m_data.maxLevelUnlocked;
    root["CurrentLanguage"] = m_data.currentLanguage;
    root["BGMVolume"] = m_data.bgmVolume;
    root["SFXVolume"] = m_data.sfxVolume;

    std::ofstream file(kSavePath);
    if (!file.is_open()) {
        TraceLog(LOG_WARNING, "SaveManager: no se pudo escribir '%s'", kSavePath);
        return;
    }
    file << root.dump(2);
}
