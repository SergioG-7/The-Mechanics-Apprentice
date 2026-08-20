#pragma once
#include "Actor.h"
#include "PowerUp.h"
#include "../Core/FSM/StateMachine.h"
#include "../Core/CountdownTimer.h"
#include "../Combat/Hitbox.h"

enum class PlayerState { Idle, Run, Attack, Hurt, Dash };

class Player : public Actor {
public:
    Player(Vector3 position, float maxHP, float speed, float attackDamage);
    ~Player();
    void Update(float dt) override;
    void Draw() const override;
    void TakeDamage(float amount, Vector3 knockbackDir) override;

    const Hitbox* GetActiveHitbox() const;
    void CloseAttackHitbox();

    // --- Power-ups temporales (ver PowerUp.h) ---
    // Recoger uno del mismo tipo mientras dura reinicia su temporizador; el
    // escudo es un booleano de un solo uso, no un temporizador.
    void ApplyPowerUp(PowerUpType type);

    // Penalización de velocidad de los charcos del Trapper (ver
    // CombatSystem::UpdateMudPuddles). Multiplicativa con el resto, así que
    // un Overclock activo la compensa en parte en vez de anularla.
    void ApplySlow(float duration, float multiplier);

    // Los tres los lee HudRenderer para pintar los indicadores de efecto
    // activo; el escudo no tiene tiempo restante que mostrar, solo si está.
    float GetOverclockRemaining() const { return m_overclockTimer.Remaining(); }
    float GetFrenzyRemaining() const { return m_frenzyTimer.Remaining(); }
    bool HasShield() const { return m_shieldActive; }

    // Asigna el shader a los materiales del cuerpo y del arma. La llama
    // Application tras construir el Player, una vez cargado el ShaderManager.
    void SetShader(Shader shader);

    // Reaplica AudioSettings::GetSfxVolume() a los Sound ya cargados -- el
    // slider de Efectos en Opciones puede tocarse durante una partida en
    // pausa (sin recrear el Player), así que el volumen fijado en el
    // constructor no basta por sí solo.
    void RefreshSfxVolume();

    // Valores finales de calibración del arma (ver Player::DrawWeapon).
    // Públicas por si hace falta retocarlas a ojo más adelante.
    Vector3 m_weaponScale = { 250.0f, 250.0f, 250.0f };
    Vector3 m_weaponOffset = { 0.5f, 0.5f, 0.0f };

private:
    void SetupStates();
    Vector3 ReadMovementInput() const;
    void UpdateIdle(float dt);
    void UpdateRun(float dt);
    void EnterAttack();
    void UpdateAttack(float dt);
    void EnterHurt();
    void UpdateHurt(float dt);
    void EnterDash();
    void UpdateDash(float dt);
    Hitbox SpawnAttackHitbox() const;
    void DrawWeapon(float rotationAngleDegrees) const;

    // Velocidad efectiva de este frame: la base del nivel por el bonus de
    // Overclock y por la penalización de lodo, ambos multiplicativos. Todo
    // desplazamiento del Player (correr, dash, avanzar durante el swing)
    // pasa por aquí -- si algún estado leyera m_moveSpeed directamente, ese
    // estado ignoraría los efectos temporales sin que se note hasta jugarlo.
    float CurrentMoveSpeed() const;

    // Recuperación real tras un swing: kAttackCooldown, o la mitad con
    // Frenzy. Cadencia resultante = kAttackDuration + esto.
    float CurrentAttackCooldown() const;

    // Entra en Attack si el cooldown ya expiró. Compartido por Idle y Run:
    // desde los dos se puede golpear, y los dos deben respetar el mismo ritmo.
    bool TryStartAttack();

    // Aro plano a ras de suelo bajo el Player, un color por efecto activo
    // (ver Draw) -- los tres pueden solaparse, así que se dibujan a radios
    // distintos en vez de pisarse unos a otros.
    void DrawStatusRing(float radius, Color color) const;

    StateMachine<PlayerState> m_fsm;
    Vector3 m_facingDirection{ 0.0f, 0.0f, 1.0f };
    float m_moveSpeed;
    float m_attackDamage;
    float m_attackTimer = 0.0f;
    float m_hurtTimer = 0.0f;
    static constexpr float kAttackDuration = 0.35f;
    static constexpr float kHurtDuration = 0.4f;

    // Ritmo de ataque: recuperación tras cada swing, así que la cadencia real
    // es 0.35 + 0.4 = 0.75s (0.55s con Frenzy) en vez de encadenar golpes tan
    // rápido como se pulse el botón. kAttackMoveFactor deja seguir andando
    // mientras dura el golpe (al 60%) en vez de clavar al jugador en el sitio.
    CountdownTimer m_attackCooldownTimer;
    static constexpr float kAttackCooldown = 0.4f;
    static constexpr float kAttackMoveFactor = 0.6f;

    // Efectos temporales. m_shieldActive no es un timer: dura hasta que
    // absorbe un golpe, sin límite de tiempo (ver TakeDamage).
    CountdownTimer m_overclockTimer;
    CountdownTimer m_frenzyTimer;
    CountdownTimer m_slowTimer;
    float m_slowMultiplier = 1.0f;
    bool m_shieldActive = false;
    static constexpr float kPowerUpDuration = 5.0f;
    static constexpr float kOverclockMultiplier = 1.5f;
    static constexpr float kFrenzyCooldownFactor = 0.5f;

    // Hit-flash: mismo mecanismo que Enemy (ver su comentario) -- destello
    // breve de impacto por encima de cualquier otro tinte, incluida Hurt.
    CountdownTimer m_damageFlashTimer;
    static constexpr float kDamageFlashDuration = 0.1f;

    // Dash: dirección congelada al entrar (ignora input mientras dura),
    // velocidad x3, sigue respetando colisión contra obstáculos.
    CountdownTimer m_dashCooldownTimer;
    float m_dashTimer = 0.0f;
    Vector3 m_dashDirection{};
    static constexpr float kDashDuration = 0.2f;
    static constexpr float kDashSpeedMultiplier = 3.0f;
    static constexpr float kDashCooldownDuration = 1.0f;

    Hitbox m_activeHitbox{};
    bool m_hitboxWindowOpen = false;

    Model m_model;
    Model m_weaponModel;

    Sound m_attackSound{};
    Sound m_hurtSound{};
    Sound m_dashSound{};
};
