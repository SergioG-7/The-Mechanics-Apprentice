#include "Player.h"
#include "../Combat/CollisionMath.h"
#include "raylib.h"
#include <iostream>

Player::Player(Vector3 position, float maxHP, float speed, float attackDamage)
    : Actor(position, maxHP), m_moveSpeed(speed), m_attackDamage(attackDamage) {
    SetupStates();

    m_model = LoadModel("assets/models/player/personaje/scene.gltf");

    // Mesh 3 = "Ground": el disco/pedestal circular que trae el modelo de
    // Sketchfab para su visor, sin relación con el personaje. Se descarta
    // la malla entera (no basta con un color transparente: DrawModelEx no
    // hace alpha-blending en esta pasada) y se recorta meshCount para que
    // ni el dibujado ni el UnloadModel del destructor vuelvan a tocarla.
    UnloadMesh(m_model.meshes[3]);
    m_model.meshCount = 3;

    // Sin texturas: estilo "prototipo sci-fi" a base de color sólido + tinte.
    // Sin resetear el color base, los materiales PBR originales del glTF
    // se ven negros en vez de responder al tinte de DrawModelEx.
    for (int i = 0; i < m_model.materialCount; i++) {
        m_model.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    }

    m_weaponModel = LoadModel("assets/models/player/arma/scene.gltf");
    for (int i = 0; i < m_weaponModel.materialCount; i++) {
        m_weaponModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    }

    m_attackSound = LoadSound("assets/audio/sfx/attack.wav");
    m_hurtSound = LoadSound("assets/audio/sfx/hurt.wav");
}

Player::~Player() {
    UnloadModel(m_model);
    UnloadModel(m_weaponModel);
    if (m_attackSound.frameCount > 0) UnloadSound(m_attackSound);
    if (m_hurtSound.frameCount > 0) UnloadSound(m_hurtSound);
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
        m_dashCooldown = 1.0f;
        m_fsm.ChangeState(PlayerState::Dash);
        return;
    }

    Vector3 dir = CollisionMath::Normalize2D(input);
    m_facingDirection = dir;

    TryMoveAgainstObstacles(Vector3{ dir.x * m_moveSpeed * dt, 0.0f, dir.z * m_moveSpeed * dt });

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

    TryMoveAgainstObstacles(Vector3{
        m_dashDirection.x * m_moveSpeed * kDashSpeedMultiplier * dt,
        0.0f,
        m_dashDirection.z * m_moveSpeed * kDashSpeedMultiplier * dt
    });

    if (m_dashTimer >= kDashDuration) {
        Vector3 input = ReadMovementInput();
        m_fsm.ChangeState((input.x != 0.0f || input.z != 0.0f) ? PlayerState::Run : PlayerState::Idle);
    }
}

void Player::EnterAttack() {
    std::cout << "[Combate] Golpe con llave inglesa gigante!" << std::endl;
    if (m_attackSound.frameCount > 0) PlaySound(m_attackSound);
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

void Player::EnterHurt() {
    m_hurtTimer = 0.0f;
    if (m_hurtSound.frameCount > 0) PlaySound(m_hurtSound);
}

void Player::UpdateHurt(float dt) {
    m_hurtTimer += dt;
    if (m_hurtTimer >= kHurtDuration) {
        m_fsm.ChangeState(PlayerState::Idle);
    }
}

void Player::Update(float dt) {
    if (m_dashCooldown > 0.0f) {
        m_dashCooldown -= dt;
    }
    ApplyKnockback(dt);
    m_fsm.Update(dt);
}

void Player::Draw() const {
    float rotationAngle = 0.0f;
    if (m_facingDirection.x != 0.0f || m_facingDirection.z != 0.0f) {
        rotationAngle = atan2f(m_facingDirection.x, m_facingDirection.z) * (180.0f / PI);
    }

    Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f }; // Queremos que gire sobre el eje Y (el suelo)
    Vector3 scale = { 0.02f, 0.02f, 0.02f };        

    Color tint = m_fsm.Is(PlayerState::Hurt) ? RED : BLUE;

    DrawModelEx(m_model, m_position, rotationAxis, rotationAngle, scale, tint);

    if (m_fsm.Is(PlayerState::Attack)) {
        DrawWeapon(rotationAngle);
    }
}

void Player::DrawWeapon(float rotationAngleDegrees) const {
    float rotationRadians = rotationAngleDegrees * (PI / 180.0f);
    float cosA = cosf(rotationRadians);
    float sinA = sinf(rotationRadians);

    // Offset local fijo (mano derecha del personaje), rotado con el mismo
    // ángulo que el cuerpo para que el arma acompañe hacia donde mira.
    // No hay rigging: es un attachment por código, no un hueso real.
    Vector3 localOffset{ 1.0f, 1.0f, 1.0f };
    Vector3 worldOffset{
        localOffset.x * cosA + localOffset.z * sinA,
        localOffset.y,
        -localOffset.x * sinA + localOffset.z * cosA
    };

    Vector3 weaponPosition{
        m_position.x + worldOffset.x,
        m_position.y + worldOffset.y,
        m_position.z + worldOffset.z
    };

    Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };
    Vector3 scale = { 5.1f, 5.1f, 5.1f };

    DrawModelEx(m_weaponModel, weaponPosition, rotationAxis, rotationAngleDegrees, scale, YELLOW);
}

void Player::TakeDamage(float amount, Vector3 knockbackDir) {
    Actor::TakeDamage(amount, knockbackDir);
    if (IsAlive()) m_fsm.ChangeState(PlayerState::Hurt);
}

const Hitbox* Player::GetActiveHitbox() const {
    return m_hitboxWindowOpen ? &m_activeHitbox : nullptr;
}

void Player::CloseAttackHitbox() { m_hitboxWindowOpen = false; }
