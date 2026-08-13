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
};
