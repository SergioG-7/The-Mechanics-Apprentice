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

    // --- Power-ups temporales ---
    // Aplica el efecto de un power-up recogido (Overclock, Frenzy o Shield).
    void ApplyPowerUp(PowerUpType type);

    // Aplica una ralentización temporal al jugador (ej. al pisar un charco).
    void ApplySlow(float duration, float multiplier);

    // Tiempo restante de cada efecto activo, para mostrarlo en el HUD.
    float GetOverclockRemaining() const { return m_overclockTimer.Remaining(); }
    float GetFrenzyRemaining() const { return m_frenzyTimer.Remaining(); }
    bool HasShield() const { return m_shieldActive; }

    // Asigna el shader al modelo del cuerpo y del arma.
    void SetShader(Shader shader);

    // Actualiza el volumen de los sonidos según los ajustes de audio actuales.
    void RefreshSfxVolume();

    // Escala y posición del arma en la mano del jugador.
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

    // Velocidad de movimiento actual, con los efectos temporales aplicados.
    float CurrentMoveSpeed() const;

    // Cooldown de ataque actual (más corto con Frenzy activo).
    float CurrentAttackCooldown() const;

    // Intenta empezar un ataque si el cooldown ya terminó.
    bool TryStartAttack();

    // Dibuja un aro de color bajo el jugador para marcar un efecto activo.
    void DrawStatusRing(float radius, Color color) const;

    StateMachine<PlayerState> m_fsm;
    Vector3 m_facingDirection{ 0.0f, 0.0f, 1.0f };
    float m_moveSpeed;
    float m_attackDamage;
    float m_attackTimer = 0.0f;
    float m_hurtTimer = 0.0f;
    static constexpr float kAttackDuration = 0.35f;
    static constexpr float kHurtDuration = 0.4f;

    // Cooldown entre ataques; kAttackMoveFactor es la velocidad al moverse mientras se golpea.
    CountdownTimer m_attackCooldownTimer;
    static constexpr float kAttackCooldown = 0.4f;
    static constexpr float kAttackMoveFactor = 0.6f;

    // Efectos temporales activos. El escudo dura hasta absorber un golpe, no por tiempo.
    CountdownTimer m_overclockTimer;
    CountdownTimer m_frenzyTimer;
    CountdownTimer m_slowTimer;
    float m_slowMultiplier = 1.0f;
    bool m_shieldActive = false;
    static constexpr float kPowerUpDuration = 5.0f;
    static constexpr float kOverclockMultiplier = 1.5f;
    static constexpr float kFrenzyCooldownFactor = 0.5f;

    // Destello breve al recibir daño.
    CountdownTimer m_damageFlashTimer;
    static constexpr float kDamageFlashDuration = 0.1f;

    // Dash: dirección fija durante el impulso, a velocidad x3.
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
