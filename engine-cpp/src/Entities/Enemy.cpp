#include "Enemy.h"
#include "../Combat/CollisionMath.h"
#include "raylib.h"
#include <iostream>

Enemy::Enemy(Vector3 position, float maxHP, std::vector<Vector3> patrolRoute, float visionRadius,
             float speed, float attackDamage)
    : Actor(position, maxHP), m_patrolRoute(std::move(patrolRoute)), m_speed(speed),
      m_attackDamage(attackDamage), m_visionRadius(visionRadius) {
    SetupStates();
    m_model = LoadModel("assets/models/enemy/robot.glb");
}

Enemy::~Enemy() {
    UnloadModel(m_model);
}

void Enemy::SetupStates() {
    StateMachine<EnemyState>::StateCallbacks patrol;
    patrol.onUpdate = [this](float dt) { UpdatePatrol(dt); };
    m_fsm.RegisterState(EnemyState::Patrol, patrol);

    StateMachine<EnemyState>::StateCallbacks chase;
    chase.onUpdate = [this](float dt) { UpdateChase(dt); };
    m_fsm.RegisterState(EnemyState::Chase, chase);

    StateMachine<EnemyState>::StateCallbacks attack;
    attack.onEnter = [this]() { EnterAttack(); };
    attack.onUpdate = [this](float dt) { UpdateAttack(dt); };
    m_fsm.RegisterState(EnemyState::Attack, attack);

    StateMachine<EnemyState>::StateCallbacks hurt;
    hurt.onEnter = [this]() { m_hurtTimer = 0.0f; };
    hurt.onUpdate = [this](float dt) { UpdateHurt(dt); };
    m_fsm.RegisterState(EnemyState::Hurt, hurt);

    StateMachine<EnemyState>::StateCallbacks dead;
    dead.onEnter = [this]() { std::cout << "[Combate] Robot averiado desactivado." << std::endl; };
    m_fsm.RegisterState(EnemyState::Dead, dead);

    m_fsm.ChangeState(EnemyState::Patrol);
}

void Enemy::UpdatePatrol(float dt) {
    if (m_playerVisible) {
        m_fsm.ChangeState(EnemyState::Chase);
        return;
    }
    if (m_patrolRoute.empty()) return;

    Vector3 target = m_patrolRoute[m_currentPatrolIndex];
    float distSq = CollisionMath::DistanceSquared(m_position, target);

    constexpr float kArriveThreshold = 0.1f;
    if (distSq <= kArriveThreshold * kArriveThreshold) {
        m_currentPatrolIndex = (m_currentPatrolIndex + 1) % m_patrolRoute.size();
        return;
    }

    Vector3 toTarget{ target.x - m_position.x, 0.0f, target.z - m_position.z };
    Vector3 dir = CollisionMath::Normalize2D(toTarget);

    m_facingDirection = dir;

    Vector3 candidate = m_position;
    candidate.x += dir.x * m_speed * dt;
    candidate.z += dir.z * m_speed * dt;

    if (!WouldCollideWithObstacles(candidate)) {
        m_position = candidate;
    }
}

void Enemy::UpdateChase(float dt) {
    float distSq = CollisionMath::DistanceSquared(m_position, m_lastKnownPlayerPosition);
    if (distSq <= kAttackRange * kAttackRange) {
        m_fsm.ChangeState(EnemyState::Attack);
        return;
    }

    Vector3 toPlayer{
        m_lastKnownPlayerPosition.x - m_position.x, 0.0f,
        m_lastKnownPlayerPosition.z - m_position.z
    };
    Vector3 dir = CollisionMath::Normalize2D(toPlayer);

    m_facingDirection = dir;

    Vector3 candidate = m_position;
    candidate.x += dir.x * m_speed * dt;
    candidate.z += dir.z * m_speed * dt;

    if (!WouldCollideWithObstacles(candidate)) {
        m_position = candidate;
    }
}

void Enemy::EnterAttack() {
    // Ataca de inmediato al entrar (cooldown a 0), luego repite cada
    // kAttackInterval mientras el Player siga en rango.
    m_attackCooldown = 0.0f;
}

Hitbox Enemy::SpawnAttackHitbox() const {
    constexpr float kKnockbackForce = 6.0f;

    Vector3 toPlayer{
        m_lastKnownPlayerPosition.x - m_position.x, 0.0f,
        m_lastKnownPlayerPosition.z - m_position.z
    };
    Vector3 dir = CollisionMath::Normalize2D(toPlayer);

    Vector3 center{ m_position.x + dir.x * 1.0f, m_position.y, m_position.z + dir.z * 1.0f };
    Vector3 halfExtents{ 0.5f, 0.5f, 0.5f };

    Hitbox hitbox;
    hitbox.box = BoundingBox{
        Vector3{ center.x - halfExtents.x, center.y - halfExtents.y, center.z - halfExtents.z },
        Vector3{ center.x + halfExtents.x, center.y + halfExtents.y, center.z + halfExtents.z }
    };
    hitbox.damage = m_attackDamage;
    hitbox.knockbackDir = Vector3{ dir.x * kKnockbackForce, 0.0f, dir.z * kKnockbackForce };
    hitbox.remainingTime = 0.15f;
    return hitbox;
}

void Enemy::UpdateAttack(float dt) {
    m_attackCooldown -= dt;
    if (m_attackCooldown <= 0.0f) {
        m_activeHitbox = SpawnAttackHitbox();
        m_hitboxWindowOpen = true;
        m_attackCooldown = kAttackInterval;
    }

    if (m_hitboxWindowOpen) {
        m_activeHitbox.remainingTime -= dt;
        if (m_activeHitbox.remainingTime <= 0.0f) m_hitboxWindowOpen = false;
    }

    float distSq = CollisionMath::DistanceSquared(m_position, m_lastKnownPlayerPosition);
    if (distSq > kAttackRange * kAttackRange) {
        m_fsm.ChangeState(EnemyState::Chase);
    }
}

void Enemy::UpdateHurt(float dt) {
    m_hurtTimer += dt;
    if (m_hurtTimer >= kHurtDuration) {
        m_fsm.ChangeState(EnemyState::Chase);
    }
}

void Enemy::Update(float dt) {
    ApplyKnockback(dt);
    m_fsm.Update(dt);
}

void Enemy::Draw() const {
    // 1. Calcular rotación para que mire hacia donde camina
    float rotationAngle = 0.0f;
    if (m_facingDirection.x != 0.0f || m_facingDirection.z != 0.0f) {
        rotationAngle = atan2f(m_facingDirection.x, m_facingDirection.z) * (180.0f / PI);
    }

    // 2. Eje de rotación (Y) y Escala
    Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };
    Vector3 scale = { 1.0f, 1.0f, 1.0f }; // Juega con esto si el robot es gigante o enano

    // 3. Sistema de daño visual (Se pone rojo al recibir un golpe)
    Color tint = m_fsm.Is(EnemyState::Hurt) ? RED : WHITE;

    // 4. Dibujado final
    DrawModelEx(m_model, m_position, rotationAxis, rotationAngle, scale, tint);
}

void Enemy::TakeDamage(float amount, Vector3 knockbackDir) {
    if (!IsAlive()) return;
    Actor::TakeDamage(amount, knockbackDir);
    m_fsm.ChangeState(IsAlive() ? EnemyState::Hurt : EnemyState::Dead);
}

void Enemy::NotifyPlayerPosition(Vector3 playerPosition) {
    m_playerVisible = CollisionMath::IsWithinRadius(m_position, playerPosition, m_visionRadius);
    if (m_playerVisible) {
        m_lastKnownPlayerPosition = playerPosition;
    }
}

const Hitbox* Enemy::GetActiveHitbox() const {
    return m_hitboxWindowOpen ? &m_activeHitbox : nullptr;
}

void Enemy::CloseAttackHitbox() { m_hitboxWindowOpen = false; }
