#include "Player.h"
#include "../Combat/CollisionMath.h"
#include "../Combat/CombatSystem.h"
#include "../Core/AudioSettings.h"
#include "../Renderer/ModelUtils.h"
#include "raylib.h"
#include <iostream>

Player::Player(Vector3 position, float maxHP, float speed, float attackDamage)
    : Actor(position, maxHP), m_moveSpeed(speed), m_attackDamage(attackDamage) {
    SetupStates();

    m_model = LoadModel("assets/models/player/character_g.glb");

    m_weaponModel = LoadModel("assets/models/player/arma/scene.gltf");
    for (int i = 0; i < m_weaponModel.materialCount; i++) {
        m_weaponModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    }

    m_attackSound = LoadSound("assets/audio/sfx/attack_player.ogg");
    m_hurtSound = LoadSound("assets/audio/sfx/hurt_player.wav");
    m_dashSound = LoadSound("assets/audio/sfx/dash_player.wav");
    RefreshSfxVolume();
}

void Player::RefreshSfxVolume() {
    float sfxVolume = AudioSettings::GetSfxVolume();
    if (m_attackSound.frameCount > 0) SetSoundVolume(m_attackSound, sfxVolume);
    if (m_hurtSound.frameCount > 0) SetSoundVolume(m_hurtSound, sfxVolume);
    if (m_dashSound.frameCount > 0) SetSoundVolume(m_dashSound, sfxVolume);
}

Player::~Player() {
    ModelUtils::UnloadModelAndTextures(m_model);
    ModelUtils::UnloadModelAndTextures(m_weaponModel);
    if (m_attackSound.frameCount > 0) UnloadSound(m_attackSound);
    if (m_hurtSound.frameCount > 0) UnloadSound(m_hurtSound);
    if (m_dashSound.frameCount > 0) UnloadSound(m_dashSound);
}

void Player::SetShader(Shader shader) {
    ModelUtils::ApplyShaderToMaterials(m_model, shader);
    ModelUtils::ApplyShaderToMaterials(m_weaponModel, shader);
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

    // Input del mando, sumado al del teclado, con zona muerta para evitar drift.
    if (IsGamepadAvailable(0)) {
        constexpr float kDeadzone = 0.25f;
        float axisX = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_X);
        float axisY = GetGamepadAxisMovement(0, GAMEPAD_AXIS_LEFT_Y);
        if (fabsf(axisX) > kDeadzone) dir.x += axisX;
        if (fabsf(axisY) > kDeadzone) dir.z += axisY;
    }

    return dir;
}

float Player::CurrentMoveSpeed() const {
    float speed = m_moveSpeed;
    if (m_overclockTimer.IsActive()) speed *= kOverclockMultiplier;
    if (m_slowTimer.IsActive()) speed *= m_slowMultiplier;
    return speed;
}

float Player::CurrentAttackCooldown() const {
    return m_frenzyTimer.IsActive() ? kAttackCooldown * kFrenzyCooldownFactor : kAttackCooldown;
}

bool Player::TryStartAttack() {
    bool attackPressed = IsKeyPressed(KEY_SPACE)
        || (IsGamepadAvailable(0) && IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_DOWN));
    if (!attackPressed || m_attackCooldownTimer.IsActive()) return false;

    m_fsm.ChangeState(PlayerState::Attack);
    return true;
}

void Player::ApplyPowerUp(PowerUpType type) {
    switch (type) {
        case PowerUpType::Overclock: m_overclockTimer.Start(kPowerUpDuration); break;
        case PowerUpType::Frenzy:    m_frenzyTimer.Start(kPowerUpDuration); break;
        case PowerUpType::Shield:    m_shieldActive = true; break;
    }
}

void Player::ApplySlow(float duration, float multiplier) {
    m_slowTimer.Start(duration);
    m_slowMultiplier = multiplier;
}

void Player::UpdateIdle(float) {
    if (TryStartAttack()) return;

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

    bool gamepadReady = IsGamepadAvailable(0);

    bool dashPressed = IsKeyPressed(KEY_LEFT_SHIFT) || IsKeyPressed(KEY_RIGHT_SHIFT)
        || (gamepadReady && (IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_TRIGGER_1)
                              || IsGamepadButtonPressed(0, GAMEPAD_BUTTON_RIGHT_FACE_RIGHT)));
    if (dashPressed && !m_dashCooldownTimer.IsActive()) {
        m_dashCooldownTimer.Start(kDashCooldownDuration);
        m_fsm.ChangeState(PlayerState::Dash);
        return;
    }

    Vector3 dir = CollisionMath::Normalize2D(input);
    m_facingDirection = dir;

    float speed = CurrentMoveSpeed();
    TryMoveAgainstObstacles(Vector3{ dir.x * speed * dt, 0.0f, dir.z * speed * dt });

    TryStartAttack();
}

void Player::EnterDash() {
    m_dashTimer = 0.0f;
    m_dashDirection = m_facingDirection; // congelada al entrar: ignora input durante el dash
    if (m_dashSound.frameCount > 0) PlaySound(m_dashSound);
}

void Player::UpdateDash(float dt) {
    m_dashTimer += dt;

    float speed = CurrentMoveSpeed() * kDashSpeedMultiplier;
    TryMoveAgainstObstacles(Vector3{ m_dashDirection.x * speed * dt, 0.0f, m_dashDirection.z * speed * dt });

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
    constexpr float kKnockbackForce = 6.0f; // fuerza inicial del empuje al golpear
    return CombatSystem::BuildMeleeHitbox(m_position, m_facingDirection, m_attackDamage, kKnockbackForce);
}

void Player::UpdateAttack(float dt) {
    m_attackTimer += dt;

    if (m_hitboxWindowOpen) {
        m_activeHitbox.remainingTime -= dt;
        if (m_activeHitbox.remainingTime <= 0.0f) m_hitboxWindowOpen = false;
    }

    // Se puede seguir andando (más despacio) mientras dura el golpe, sin girar la hitbox ya calculada.
    Vector3 input = ReadMovementInput();
    if (input.x != 0.0f || input.z != 0.0f) {
        Vector3 dir = CollisionMath::Normalize2D(input);
        float speed = CurrentMoveSpeed() * kAttackMoveFactor;
        TryMoveAgainstObstacles(Vector3{ dir.x * speed * dt, 0.0f, dir.z * speed * dt });
    }

    if (m_attackTimer >= kAttackDuration) {
        m_attackCooldownTimer.Start(CurrentAttackCooldown());
        m_fsm.ChangeState((input.x != 0.0f || input.z != 0.0f) ? PlayerState::Run : PlayerState::Idle);
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
    m_dashCooldownTimer.Tick(dt);
    m_damageFlashTimer.Tick(dt);
    m_attackCooldownTimer.Tick(dt);
    m_overclockTimer.Tick(dt);
    m_frenzyTimer.Tick(dt);
    m_slowTimer.Tick(dt);
    ApplyKnockback(dt);
    m_fsm.Update(dt);
}

void Player::Draw() const {
    float rotationAngle = CollisionMath::HeadingDegrees(m_facingDirection);

    Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f }; // Queremos que gire sobre el eje Y (el suelo)
    Vector3 scale = { 1.0f, 1.0f, 1.0f };        

    Color tint = m_fsm.Is(PlayerState::Hurt) ? RED : WHITE;

    // El tinte de un efecto activo (lodo, Overclock) sustituye al del estado.
    if (m_slowTimer.IsActive())      tint = Color{ 150, 230, 150, 255 };
    if (m_overclockTimer.IsActive()) tint = PowerUp::TypeColor(PowerUpType::Overclock);

    // Destello breve al recibir daño, por encima de cualquier otro tinte.
    if (m_damageFlashTimer.IsActive()) {
        tint = WHITE;
    }

    // Sombra falsa en el suelo.
    DrawCylinder(Vector3{ m_position.x, 0.01f, m_position.z }, 0.6f, 0.6f, 0.01f, 15, Color{ 0, 0, 0, 100 });

    // Un aro por efecto activo, a radios distintos para que no se tapen entre sí.
    if (m_overclockTimer.IsActive()) DrawStatusRing(0.9f, PowerUp::TypeColor(PowerUpType::Overclock));
    if (m_frenzyTimer.IsActive())    DrawStatusRing(1.1f, PowerUp::TypeColor(PowerUpType::Frenzy));
    if (m_slowTimer.IsActive())      DrawStatusRing(1.3f, Color{ 80, 200, 60, 255 });

    // Dibuja el cuerpo con un contorno estilo anime.
    ModelUtils::DrawModelWithOutline(m_model, m_position, rotationAxis, rotationAngle, scale, tint);

    if (m_fsm.Is(PlayerState::Attack)) {
        DrawWeapon(rotationAngle);
    }

    // Burbuja de escudo, encima del modelo.
    if (m_shieldActive) {
        Color shieldColor = PowerUp::TypeColor(PowerUpType::Shield);
        DrawSphereWires(Vector3{ m_position.x, m_position.y + 0.6f, m_position.z }, 1.0f, 8, 10, shieldColor);
    }
}

void Player::DrawStatusRing(float radius, Color color) const {
    Vector3 base{ m_position.x, 0.03f, m_position.z };
    DrawCylinderWires(base, radius, radius, 0.02f, 24, color);
}

void Player::DrawWeapon(float rotationAngleDegrees) const {
    float rotationRadians = rotationAngleDegrees * (PI / 180.0f);
    float cosA = cosf(rotationRadians);
    float sinA = sinf(rotationRadians);

    // Rota la posición del arma con el mismo ángulo que el cuerpo, para que lo acompañe.
    Vector3 worldOffset{
        m_weaponOffset.x * cosA + m_weaponOffset.z * sinA,
        m_weaponOffset.y,
        -m_weaponOffset.x * sinA + m_weaponOffset.z * cosA
    };

    Vector3 weaponPosition{
        m_position.x + worldOffset.x,
        m_position.y + worldOffset.y,
        m_position.z + worldOffset.z
    };

    Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };

    DrawModelEx(m_weaponModel, weaponPosition, rotationAxis, rotationAngleDegrees, m_weaponScale, LIGHTGRAY);
}

void Player::TakeDamage(float amount, Vector3 knockbackDir) {
    // El escudo absorbe el golpe entero: sin daño, sin empuje, solo un destello.
    if (m_shieldActive) {
        m_shieldActive = false;
        m_damageFlashTimer.Start(kDamageFlashDuration);
        return;
    }

    Actor::TakeDamage(amount, knockbackDir);
    m_damageFlashTimer.Start(kDamageFlashDuration);
    if (IsAlive()) m_fsm.ChangeState(PlayerState::Hurt);
}

const Hitbox* Player::GetActiveHitbox() const {
    return m_hitboxWindowOpen ? &m_activeHitbox : nullptr;
}

void Player::CloseAttackHitbox() { m_hitboxWindowOpen = false; }
