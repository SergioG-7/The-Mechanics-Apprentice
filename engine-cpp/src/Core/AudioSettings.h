#pragma once

// Volumen de SFX global (golpes, daño, UI), consultado por Player/Enemy al
// cargar sus propios Sound (SetSoundVolume) y al reaplicarlo en caliente
// (ver Player::RefreshSfxVolume/Enemy::RefreshSfxVolume) cuando el usuario
// mueve el slider de Opciones durante una partida en pausa. Estático porque
// Player/Enemy no tienen (ni deben tener) una referencia a SaveManager --
// solo Application la tiene, y es quien mantiene esto sincronizado con
// SaveManager::Data().sfxVolume. La música NO pasa por aquí: MusicController
// guarda su propio volumen directamente (ver MusicController::SetVolume).
namespace AudioSettings {
    void SetSfxVolume(float volume);
    float GetSfxVolume();
}
