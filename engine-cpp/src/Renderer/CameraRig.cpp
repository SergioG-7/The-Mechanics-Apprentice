#include "CameraRig.h"

CameraRig::CameraRig() {
    m_camera.position = { 0.0f, kHeight, kDistance };
    m_camera.target = { 0.0f, 1.0f, 0.0f };
    m_camera.up = { 0.0f, 1.0f, 0.0f };
    m_camera.fovy = 45.0f;
    m_camera.projection = CAMERA_PERSPECTIVE;
}

void CameraRig::FollowTarget(Vector3 target) {
    m_camera.target = target;
    m_camera.position = Vector3{ target.x, target.y + kHeight, target.z + kDistance };

    if (!m_shakeTimer.IsActive()) return;

    m_camera.position.x += (static_cast<float>(GetRandomValue(-100, 100)) / 100.0f) * m_shakeIntensity;
    m_camera.position.z += (static_cast<float>(GetRandomValue(-100, 100)) / 100.0f) * m_shakeIntensity;
}

void CameraRig::AddShake(float duration, float intensity) {
    m_shakeTimer.Start(duration);
    m_shakeIntensity = intensity;
}

void CameraRig::ClearShake() {
    m_shakeTimer.Start(0.0f);
    m_shakeIntensity = 0.0f;
}
