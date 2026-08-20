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

    // Modelo Kenney (blocky-characters, variante G): trae su propio atlas
    // vía pbrMetallicRoughness.baseColorTexture, que raylib sí resuelve solo
    // (a diferencia del workflow specular-glossiness de los modelos
    // Sketchfab anteriores). No hace falta inyectar textura a mano ni tocar
    // el color base del material -- ya nace en blanco, así el toon shader
    // recibe el atlas sin teñir.
    m_model = LoadModel("assets/models/player/character_g.glb");

    m_weaponModel = LoadModel("assets/models/player/arma/scene.gltf");
    for (int i = 0; i < m_weaponModel.materialCount; i++) {
        m_weaponModel.materials[i].maps[MATERIAL_MAP_ALBEDO].color = WHITE;
    }

    // Nombres reales en disco -- "attack.ogg"/"hurt.ogg" (de una sesión
    // anterior) ya no existen ahí, así que LoadSound fallaba en silencio
    // (frameCount 0, guardado por el "if" de abajo) y estos dos sonidos
    // nunca llegaban a sonar.
    m_attackSound = LoadSound("assets/audio/sfx/attack_player.ogg");
    m_hurtSound = LoadSound("assets/audio/sfx/hurt_player.wav");
    m_dashSound = LoadSound("assets/audio/sfx/dash_player.wav");
    RefreshSfxVolume();
}

void Player::RefreshSfxVolume() {
    // Directo, sin multiplicador propio de por medio: si la UI manda 1.0
    // (SFX al 100%), SetSoundVolume tiene que recibir 1.0, no una fracción
    // atenuada por un "balance" interno que el slider no puede compensar.
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

    // Mando: se fusiona con el teclado, no lo sustituye (jugar con los dos a
    // la vez no se rompe -- solo se suma). Deadzone para no arrastrar drift
    // de un stick mal centrado como si fuera input real. La magnitud
    // combinada puede superar 1 si se usan ambos a la vez, pero da igual:
    // UpdateRun normaliza el resultado, solo le importa la dirección.
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

    // El cooldown NO arranca aquí sino al terminar el swing (ver
    // UpdateAttack): es un tiempo de recuperación ENTRE golpes. Arrancándolo
    // al entrar, los 0.35s de animación se comerían casi todo el 0.4s y ni
    // el cooldown frenaría el spam ni Frenzy se notaría al reducirlo.
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
    // Atacar sin moverse: antes solo se podía desde Run, así que golpear
    // desde parado obligaba a dar un paso primero.
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
    // !IsActive(): el cooldown se fijaba pero nunca se comprobaba -- se podía
    // encadenar un dash tras otro tan rápido como el jugador pulsase la
    // tecla, sin ningún respiro real entre ellos.
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
    // Fuerza de knockback real (antes el vector iba sin escalar, magnitud
    // ~1.0 -- apenas empujaba). kKnockbackForce en unidades/seg de impulso
    // inicial; Actor::ApplyKnockback ya lo frena con drag.
    constexpr float kKnockbackForce = 6.0f;
    return CombatSystem::BuildMeleeHitbox(m_position, m_facingDirection, m_attackDamage, kKnockbackForce);
}

void Player::UpdateAttack(float dt) {
    m_attackTimer += dt;

    if (m_hitboxWindowOpen) {
        m_activeHitbox.remainingTime -= dt;
        if (m_activeHitbox.remainingTime <= 0.0f) m_hitboxWindowOpen = false;
    }

    // Se puede seguir andando mientras dura el swing, a velocidad reducida:
    // antes el ataque clavaba al jugador en el sitio los 0.35s enteros. La
    // dirección de encaramiento NO se toca -- la hitbox ya se calculó al
    // entrar (EnterAttack), así que girar aquí movería el golpe a mitad de
    // animación sin que la caja lo siguiera.
    Vector3 input = ReadMovementInput();
    if (input.x != 0.0f || input.z != 0.0f) {
        Vector3 dir = CollisionMath::Normalize2D(input);
        float speed = CurrentMoveSpeed() * kAttackMoveFactor;
        TryMoveAgainstObstacles(Vector3{ dir.x * speed * dt, 0.0f, dir.z * speed * dt });
    }

    if (m_attackTimer >= kAttackDuration) {
        // Se lee CurrentAttackCooldown() ahora, no al empezar el swing: si
        // Frenzy expiró a mitad del golpe, la recuperación ya es la normal.
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

    // El límite del mapa ya no es un clamp aquí -- son muros de Obstacle
    // reales alrededor del perímetro de cada nivel (ver assets/data/*.json),
    // así que la colisión normal contra obstáculos (TryMoveAgainstObstacles,
    // más arriba en cada Update* de estado) ya se encarga.
}

void Player::Draw() const {
    float rotationAngle = 0.0f;
    if (m_facingDirection.x != 0.0f || m_facingDirection.z != 0.0f) {
        rotationAngle = atan2f(m_facingDirection.x, m_facingDirection.z) * (180.0f / PI);
    }

    Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f }; // Queremos que gire sobre el eje Y (el suelo)
    Vector3 scale = { 1.0f, 1.0f, 1.0f };        

    // WHITE en normal para no teñir el atlas de Kenney (el toon shader ya
    // procesa sus colores tal cual); el flash de daño pasa a RED, ya que
    // WHITE dejaría de contrastar contra un tinte neutro.
    Color tint = m_fsm.Is(PlayerState::Hurt) ? RED : WHITE;

    // Tinte de efecto temporal por encima del de estado: el lodo apaga a
    // verde y el Overclock enciende a amarillo (y gana si coinciden, que es
    // justo lo que el jugador necesita ver: el acelerón sigue activo).
    if (m_slowTimer.IsActive())      tint = Color{ 150, 230, 150, 255 };
    if (m_overclockTimer.IsActive()) tint = PowerUp::TypeColor(PowerUpType::Overclock);

    // Hit-flash: destello breve por encima del tinte de estado -- ver el
    // mismo mecanismo en Enemy::Draw.
    if (m_damageFlashTimer.IsActive()) {
        tint = WHITE;
    }

    // Sombra falsa: ancla al personaje al suelo sin necesitar un shader de
    // sombras real. Y a 0.01f para evitar z-fighting con el suelo.
    DrawCylinder(Vector3{ m_position.x, 0.01f, m_position.z }, 0.6f, 0.6f, 0.01f, 15, Color{ 0, 0, 0, 100 });

    // Aros de estado a radios distintos para que dos efectos simultáneos se
    // vean los dos, en vez de dibujarse uno encima del otro.
    if (m_overclockTimer.IsActive()) DrawStatusRing(0.9f, PowerUp::TypeColor(PowerUpType::Overclock));
    if (m_frenzyTimer.IsActive())    DrawStatusRing(1.1f, PowerUp::TypeColor(PowerUpType::Frenzy));
    if (m_slowTimer.IsActive())      DrawStatusRing(1.3f, Color{ 80, 200, 60, 255 });

    // Outline estilo anime ("inverted hull") -- ver ModelUtils::DrawModelWithOutline.
    // Negro puro (sin depender de la iluminación del shader: negro × cualquier
    // color = negro) y con el mismo alpha que el cuerpo, para que no quede un
    // borde sólido si el cuerpo se desvanece.
    ModelUtils::DrawModelWithOutline(m_model, m_position, rotationAxis, rotationAngle, scale, tint);

    if (m_fsm.Is(PlayerState::Attack)) {
        DrawWeapon(rotationAngle);
    }

    // Escudo: burbuja de alambre alrededor del cuerpo. Se dibuja el último,
    // por encima del modelo, para que se lea como una capa que lo envuelve.
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

    // m_weaponOffset es local a la mano del personaje; se rota con el mismo
    // ángulo que el cuerpo para que el arma acompañe hacia donde mira. No
    // hay rigging: es un attachment por código, no un hueso real.
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
    // Escudo (batería): absorbe el golpe entero -- ni HP, ni empuje, ni
    // estado Hurt. Solo el destello de impacto, para que se vea que ha
    // conectado algo y que el escudo se acaba de gastar.
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
