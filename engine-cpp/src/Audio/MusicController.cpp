#include "MusicController.h"

MusicController::MusicController(const std::string& path, float volume) {
    m_music = LoadMusicStream(path.c_str());
    m_loaded = m_music.frameCount > 0;
    if (m_loaded) {
        m_music.looping = true;
        SetMusicVolume(m_music, volume);
    }
}

MusicController::~MusicController() {
    if (m_loaded) UnloadMusicStream(m_music);
}

void MusicController::Play() {
    if (m_loaded) PlayMusicStream(m_music);
}

void MusicController::Stop() {
    if (m_loaded) StopMusicStream(m_music);
}

void MusicController::Update() {
    if (m_loaded) UpdateMusicStream(m_music);
}

void MusicController::SetVolume(float volume) {
    if (m_loaded) SetMusicVolume(m_music, volume);
}
