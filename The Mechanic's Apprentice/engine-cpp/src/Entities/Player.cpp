#include "Player.h"
#include "../Combat/CollisionMath.h"
#include "raylib.h"
#include <iostream>

Player::Player(Vector3 position, float maxHP, float speed, float attackDamage)
    : Actor(position, maxHP), m_moveSpeed(speed), m_attackDamage(attackDamage) {
    SetupStates();
}

void Player::SetupStates() {
    StateMachine<PlayerState>::StateCallbacks idle;
    idle.onUpdate = [this](float dt) { UpdateIdle(dt); };
    m_fsm.RegisterState(PlayerState::Idle, idle);

    StateMachine<PlayerState>::StateCallbacks run;
    run.onUpdate = [this](float dt) { UpdateRun(dt); };
    m_fsm.RegisterState(PlayerState::Run, run);

    StateMachine<PlayerState>::StateCallbacks attack;
    attack.onEnter = [this]() { EnterAttack(); };
    attack.onUpdate = [this](float dt) { UpdateAttack(dt); };
    m_fsm.RegisterState(PlayerState::Attack, attack);

    StateMachine<PlayerState>::StateCallbacks hurt;
    hurt.onEnter = [this]() { EnterHurt(); };
    hurt.onUpdate = [this](float dt) { UpdateHurt(dt); };
    m_fsm.RegisterState(PlayerState::Hurt, hurt);

    StateMachine<PlayerState>::StateCallbacks dash;
    dash.onEnter = [this]() { EnterDash(); };
    dash.onUpdate = [this](float dt) { UpdateDash(dt); };
    m_fsm.RegisterState(PlayerState::Dash, dash);

    m_fsm.ChangeState(PlayerState::Idle);
}

Vector3 Player::ReadMovementInput() const {
    Vector3 dir{ 0.0f, 0.0f, 0.0f };
    if (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP))    dir.z -= 1.0f;
    if (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN))  dir.z += 1.0f;
    if (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT))  dir.x -= 1.0f;
    if (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT)) dir.x += 1.0f;
    return dir;
}

void Player::UpdateIdle(float dt) {
    Vector3 input = ReadMovementInput();
    if (input.x != 0.0f || input.z != 0.0f) {
        m_fsm.ChangeState(PlayerState::Run);
    }
}

void Player::UpdateRun(float dt) {
    Vector3 input = ReadMovementInput();

    if (input.x == 0.0f && input.z == 0.0f) {
        m_fsm.ChangeState(PlayerState::Idle);
        return;
    }

    if (IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT)) {
        m_fsm.ChangeState(PlayerState::Dash);
        return;
    }

    Vector3 dir = CollisionMath::Normalize2D(input);
    m_facingDirection = dir;

    Vector3 candidate = m_position;
    candidate.x += dir.x * m_moveSpeed * dt;
    candidate.z += dir.z * m_moveSpeed * dt;

    if (!WouldCollideWithObstacles(candidate)) {
        m_position = candidate;
    }

    if (IsKeyPressed(KEY_SPACE)) {
        m_fsm.ChangeState(PlayerState::Attack);
    }
}

void Player::EnterDash() {
    m_dashTimer = 0.0f;
    m_dashDirection = m_facingDirection; // congelada al entrar: ignora input durante el dash
}

void Player::UpdateDash(float dt) {
    m_dashTimer += dt;

    Vector3 candidate = m_position;
    candidate.x += m_dashDirection.x * m_moveSpeed * kDashSpeedMultiplier * dt;
    candidate.z += m_dashDirection.z * m_moveSpeed * kDashSpeedMultiplier * dt;

    if (!WouldCollideWithObstacles(candidate)) {
        m_position = candidate;
    }

    if (m_dashTimer >= kDashDuration) {
        Vector3 input = ReadMovementInput();
        m_fsm.ChangeState((input.x != 0.0f || input.z != 0.0f) ? PlayerState::Run : PlayerState::Idle);
    }
}

void Player::EnterAttack() {
    std::cout << "[Combate] Golpe con llave inglesa gigante!" << std::endl;
    m_attackTimer = 0.0f;
    m_activeHitbox = SpawnAttackHitbox();
    m_hitboxWindowOpen = true;
}

Hitbox Player::SpawnAttackHitbox() const {
    // Fuerza de knockback real (antes el vector iba sin escalar, magnitud
    // ~1.0 -- apenas empujaba). kKnockbackForce en unidades/seg de impulso
    // inicial; Actor::ApplyKnockback ya lo frena con drag.
    constexpr float kKnockbackForce = 6.0f;

    Vector3 center{
        m_position.x + m_facingDirection.x * 1.0f,
        m_position.y,
        m_position.z + m_facingDirection.z * 1.0f
    };
    Vector3 halfExtents{ 0.5f, 0.5f, 0.5f };

    Hitbox hitbox;
    hitbox.box = BoundingBox{
        Vector3{ center.x - halfExtents.x, center.y - halfExtents.y, center.z - halfExtents.z },
        Vector3{ center.x + halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z }
    };
    hitbox.damage = m_attackDamage;
    hitbox.knockbackDir = Vector3{ m_facingDirection.x * kKnockbackForce, 0.0f, m_facingDirection.z * kKnockbackForce };
    hitbox.remainingTime = 0.15f;
    return hitbox;
}

void Player::UpdateAttack(float dt) {
    m_attackTimer += dt;

    if (m_hitboxWindowOpen) {
        m_activeHitbox.remainingTime -= dt;
        if (m_activeHitbox.remainingTime <= 0.0f) m_hitboxWindowOpen = false;
    }

    if (m_attackTimer >= kAttackDuration) {
        m_fsm.ChangeState(PlayerState::Idle);
    }
}

void Player::EnterHurt() { m_hurtTimer = 0.0f; }

void Player::UpdateHurt(float dt) {
    m_hurtTimer += dt;
    if (m_hurtTimer >= kHurtDuration) {
        m_fsm.ChangeState(PlayerState::Idle);
    }
}

void Player::Update(float dt) {
    ApplyKnockback(dt);
    m_fsm.Update(dt);
}

void Player::Draw() const {
    Color tint = m_fsm.Is(PlayerState::Hurt) ? Color{ 255, 60, 60, 255 } : BLUE;
    DrawCube(m_position, m_halfExtents.x * 2.0f, m_halfExtents.y * 2.0f, m_halfExtents.z * 2.0f, tint);
}

void Player::TakeDamage(float amount, Vector3 knockbackDir) {
    Actor::TakeDamage(amount, knockbackDir);
    if (IsAlive()) m_fsm.ChangeState(PlayerState::Hurt);
}

const Hitbox* Player::GetActiveHitbox() const {
    return m_hitboxWindowOpen ? &m_activeHitbox : nullptr;
}

void Player::CloseAttackHitbox() { m_hitboxWindowOpen = false; }
