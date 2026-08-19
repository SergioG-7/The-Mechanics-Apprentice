#pragma once
#include "raylib.h"
#include <string>

// Wrapper RAII sobre la música de fondo: Application llamaba directamente a
// Load/Play/Stop/Update/UnloadMusicStream repartidos por medio archivo (el
// mismo "if (frameCount > 0)" repetido en seis sitios distintos). Aquí
// quedan agrupados con el propio Music que gobiernan.
class MusicController {
public:
    MusicController(const std::string& path, float volume);
    ~MusicController();

    MusicController(const MusicController&) = delete;
    MusicController& operator=(const MusicController&) = delete;

    void Play();
    void Stop();
    void Update();

private:
    Music m_music{};
    bool m_loaded = false;
};
