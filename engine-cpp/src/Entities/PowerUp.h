#pragma once
#include "Entity.h"
#include <string>

// Los tres efectos temporales que puede otorgar un PowerUp. El nombre que
// aparece en el JSON de nivel ("Overclock"/"Frenzy"/"Shield") lo resuelve
// ParseType; el motor nunca guarda la cadena, solo este enum.
enum class PowerUpType { Overclock, Frenzy, Shield };

// Objeto de recolección que aplica un efecto temporal al Player al tocarlo
// (ver Player::ApplyPowerUp). Sin HP ni FSM, igual que Gear/HealthKit:
// hereda de Entity, no de Actor. Se dibuja con geometría de código (no hay
// modelo propio), una forma distinta por tipo para que se distingan de un
// vistazo aunque el color no se aprecie.
class PowerUp : public Entity {
public:
    PowerUp(Vector3 position, PowerUpType type);

    void Update(float) override {}
    void Draw() const override;

    PowerUpType GetType() const { return m_type; }

    // Nombre desconocido (typo, JSON viejo) cae a Overclock -- mismo criterio
    // de degradar sin reventar que EnemyFactory::ParseBehavior.
    static PowerUpType ParseType(const std::string& name);

    // Color de identidad de cada tipo, compartido por el propio pickup, el
    // aura del Player mientras dura el efecto y el indicador del HUD -- un
    // solo sitio para que los tres no puedan desincronizarse.
    static Color TypeColor(PowerUpType type);

private:
    PowerUpType m_type;
};
