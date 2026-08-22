#pragma once
#include "raylib.h"
#include <string>

// Controla la música de fondo: carga, reproduce y libera el stream.
class MusicController {
public:
    MusicController(const std::string& path, float volume);
    ~MusicController();

    MusicController(const MusicController&) = delete;
    MusicController& operator=(const MusicController&) = delete;

    void Play();
    void Stop();
    void Update();

    // Cambia el volumen de la música ya en reproducción.
    void SetVolume(float volume);

private:
    Music m_music{};
    bool m_loaded = false;
};
