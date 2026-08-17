#pragma once
#include "Actor.h"
#include "../Core/FSM/StateMachine.h"
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

    // Asigna el shader a los materiales del cuerpo y del arma. La llama
    // Application tras construir el Player, una vez cargado el ShaderManager.
    void SetShader(Shader shader);

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

    StateMachine<PlayerState> m_fsm;
    Vector3 m_facingDirection{ 0.0f, 0.0f, 1.0f };
    float m_moveSpeed;
    float m_attackDamage;
    float m_attackTimer = 0.0f;
    float m_hurtTimer = 0.0f;
    static constexpr float kAttackDuration = 0.35f;
    static constexpr float kHurtDuration = 0.4f;

    // Hit-flash: mismo mecanismo que Enemy (ver su comentario) -- destello
    // breve de impacto por encima de cualquier otro tinte, incluida Hurt.
    float m_damageFlashTimer = 0.0f;
    static constexpr float kDamageFlashDuration = 0.1f;

    // Dash: dirección congelada al entrar (ignora input mientras dura),
    // velocidad x3, sigue respetando colisión contra obstáculos.
    float m_dashCooldown = 0.0f;
    float m_dashTimer = 0.0f;
    Vector3 m_dashDirection{};
    static constexpr float kDashDuration = 0.2f;
    static constexpr float kDashSpeedMultiplier = 3.0f;

    Hitbox m_activeHitbox{};
    bool m_hitboxWindowOpen = false;

    Model m_model;
    Model m_weaponModel;

    Sound m_attackSound{};
    Sound m_hurtSound{};
    static constexpr float kAttackSoundVolume = 0.5f; // 0.0 (silencio) - 1.0 (volumen original del archivo)
    static constexpr float kHurtSoundVolume = 0.5f;
};
