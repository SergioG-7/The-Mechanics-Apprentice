#pragma once
#include "Entity.h"
#include <string>

// Los tres efectos temporales que puede dar un power-up.
enum class PowerUpType { Overclock, Frenzy, Shield };

// Objeto de recolección que aplica un efecto temporal al jugador al tocarlo.
class PowerUp : public Entity {
public:
    PowerUp(Vector3 position, PowerUpType type);

    void Update(float) override {}
    void Draw() const override;

    PowerUpType GetType() const { return m_type; }

    // Convierte el nombre del JSON de nivel en un PowerUpType.
    static PowerUpType ParseType(const std::string& name);

    // Color de identidad de cada tipo, usado en el pickup, el aura del jugador y el HUD.
    static Color TypeColor(PowerUpType type);

private:
    PowerUpType m_type;
};
