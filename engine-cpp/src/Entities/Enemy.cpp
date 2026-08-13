#include "Enemy.h"
#include "../Combat/CollisionMath.h"
#include "raylib.h"
#include <iostream>

Enemy::Enemy(Vector3 position, float maxHP, std::vector<Vector3> patrolRoute, float visionRadius,
             float speed, float attackDamage)
    : Actor(position, maxHP), m_patrolRoute(std::move(patrolRoute)), m_speed(speed),
      m_attackDamage(attackDamage), m_visionRadius(visionRadius) {
    SetupStates();

    m_model = LoadModel("assets/models/enemy/scene.gltf");
    // Sin texturas: estilo "prototipo sci-fi" a base de color sólido + tinte.
    // Sin resetear el color base, los materiales PBR originales del glTF
    // se ven negros en vez de responder al tinte de DrawModelEx.
    for (int i = 0; i < m_model.materialCount; i++) {
        m_model.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    }

    m_hurtSound = LoadSound("assets/audio/sfx/hurt.ogg");
    if (m_hurtSound.frameCount > 0) SetSoundVolume(m_hurtSound, kHurtSoundVolume);
}

Enemy::~Enemy() {
    UnloadModel(m_model);
    if (m_hurtSound.frameCount > 0) UnloadSound(m_hurtSound);
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
    hurt.onEnter = [this]() {
        m_hurtTimer = 0.0f;
        if (m_hurtSound.frameCount > 0) PlaySound(m_hurtSound);
    };
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

    TryMoveAgainstObstacles(Vector3{ dir.x * m_speed * dt, 0.0f, dir.z * m_speed * dt });
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

    TryMoveAgainstObstacles(Vector3{ dir.x * m_speed * dt, 0.0f, dir.z * m_speed * dt });
}

void Enemy::EnterAttack() {
    // No resetear m_attackCooldown aquí: si el jugador oscila en el borde de
    // kAttackRange, el FSM re-entra en Attack varias veces por segundo y un
    // reset a 0 dispararía un golpe instantáneo cada vez, saltándose kAttackInterval.
}

Hitbox Enemy::SpawnAttackHitbox() const {
    // Subido de 6.0f: da al jugador más distancia tras un golpe, con margen
    // real para reaccionar con el Dash en vez de quedar pegado al enemigo.
    constexpr float kKnockbackForce = 9.0f;

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
    float rotationAngle = 0.0f;
    if (m_facingDirection.x != 0.0f || m_facingDirection.z != 0.0f) {
        rotationAngle = atan2f(m_facingDirection.x, m_facingDirection.z) * (180.0f / PI);
    }

    Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };
    Vector3 scale = { 3.0f, 3.0f, 3.0f };

    // Flash blanco al recibir un golpe: el contraste extremo contra el rojo
    // base se lee mucho mejor que un simple cambio de tono. Óxido apagado
    // cuando queda desactivado.
    Color tint = RED;
    if (m_fsm.Is(EnemyState::Hurt)) {
        tint = WHITE;
    } else if (m_fsm.Is(EnemyState::Dead)) {
        tint = Color{ 80, 20, 20, 255 };
    }

    // Sombra falsa: ancla al robot al suelo sin necesitar un shader de
    // sombras real. Y a 0.01f para evitar z-fighting con el suelo.
    DrawCylinder(Vector3{ m_position.x, 0.01f, m_position.z }, 0.6f, 0.6f, 0.01f, 15, Color{ 0, 0, 0, 100 });

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
