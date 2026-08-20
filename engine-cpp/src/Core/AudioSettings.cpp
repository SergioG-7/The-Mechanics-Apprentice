#include "AudioSettings.h"

namespace {
float g_sfxVolume = 1.0f;
}

namespace AudioSettings {

void SetSfxVolume(float volume) { g_sfxVolume = volume; }
float GetSfxVolume() { return g_sfxVolume; }

} // namespace AudioSettings
