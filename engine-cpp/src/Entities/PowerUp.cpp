#include "PowerUp.h"
#include "../Core/Pulse.h"
#include "raylib.h"
#include "rlgl.h"

PowerUp::PowerUp(Vector3 position, PowerUpType type)
    : Entity(position, Vector3{ 0.4f, 0.4f, 0.4f }), m_type(type) {}

PowerUpType PowerUp::ParseType(const std::string& name) {
    if (name == "Overclock") return PowerUpType::Overclock;
    if (name == "Frenzy")    return PowerUpType::Frenzy;
    if (name == "Shield")    return PowerUpType::Shield;

    TraceLog(LOG_WARNING, "PowerUp: tipo '%s' desconocido, usando 'Overclock'", name.c_str());
    return PowerUpType::Overclock;
}

Color PowerUp::TypeColor(PowerUpType type) {
    switch (type) {
        case PowerUpType::Overclock: return Color{ 255, 220, 60, 255 };  // amarillo eléctrico
        case PowerUpType::Frenzy:    return Color{ 255, 130, 40, 255 };  // naranja de forja
        case PowerUpType::Shield:    return Color{ 90, 200, 255, 255 };  // azul batería
    }
    return WHITE;
}

void PowerUp::Draw() const {
    Color color = TypeColor(m_type);

    // Halo plano en el suelo: común a los tres tipos, marca dónde está el
    // pickup aunque la pieza flotante quede tapada por una pared cercana.
    Vector3 halo{ m_position.x, 0.02f, m_position.z };
    DrawCylinder(halo, 0.6f, 0.6f, 0.02f, 20, Fade(color, 0.25f));
    DrawCylinderWires(halo, 0.6f, 0.6f, 0.02f, 20, color);

    // Flotación + giro: mismo truco que Gear (rlRotatef sobre la matriz de
    // mundo, porque DrawCube/DrawCylinder no aceptan ángulo por parámetro).
    float bob = (Pulse::Wave01(static_cast<float>(GetTime()), 2.5f) - 0.5f) * 0.24f;

    rlPushMatrix();
    rlTranslatef(m_position.x, m_position.y + 0.6f + bob, m_position.z);
    rlRotatef(static_cast<float>(GetTime()) * 70.0f, 0.0f, 1.0f, 0.0f);

    switch (m_type) {
        case PowerUpType::Overclock: {
            // Cono apuntando hacia arriba: lectura inmediata de "acelerón".
            Vector3 base{ 0.0f, -0.3f, 0.0f };
            DrawCylinder(base, 0.32f, 0.0f, 0.7f, 12, color);
            DrawCylinderWires(base, 0.32f, 0.0f, 0.7f, 12, RAYWHITE);
            break;
        }
        case PowerUpType::Frenzy: {
            // Dos aspas planas cruzadas: forma de "más golpes por segundo".
            DrawCube(Vector3{ 0.0f, 0.0f, 0.0f }, 0.9f, 0.12f, 0.22f, color);
            DrawCube(Vector3{ 0.0f, 0.0f, 0.0f }, 0.22f, 0.12f, 0.9f, color);
            DrawCubeWires(Vector3{ 0.0f, 0.0f, 0.0f }, 0.9f, 0.12f, 0.22f, RAYWHITE);
            DrawCubeWires(Vector3{ 0.0f, 0.0f, 0.0f }, 0.22f, 0.12f, 0.9f, RAYWHITE);
            break;
        }
        case PowerUpType::Shield: {
            // Batería: cuerpo prismático con el borne encima.
            DrawCube(Vector3{ 0.0f, 0.0f, 0.0f }, 0.45f, 0.6f, 0.45f, color);
            DrawCubeWires(Vector3{ 0.0f, 0.0f, 0.0f }, 0.45f, 0.6f, 0.45f, RAYWHITE);
            DrawCube(Vector3{ 0.0f, 0.38f, 0.0f }, 0.18f, 0.16f, 0.18f, RAYWHITE);
            break;
        }
    }

    rlPopMatrix();
}
