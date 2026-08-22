#include "Enemy.h"
#include "../Combat/CollisionMath.h"
#include "../Combat/CombatSystem.h"
#include "../Core/AudioSettings.h"
#include "../Core/Pulse.h"
#include "../Renderer/ModelUtils.h"
#include "raylib.h"
#include "rlgl.h"
#include <iostream>

Enemy::Enemy(Vector3 position, float maxHP, std::vector<Vector3> patrolRoute, float visionRadius,
             float speed, float attackDamage, float scale, EnemyBehavior behavior, Color baseTint,
             float turnRateDegPerSec)
    : Actor(position, maxHP, Vector3{ 0.5f * scale, 0.5f * scale, 0.5f * scale }),
      m_patrolRoute(std::move(patrolRoute)), m_speed(speed),
      m_attackDamage(attackDamage), m_scale(scale), m_behavior(behavior), m_baseTint(baseTint),
      m_turnRateDegPerSec(turnRateDegPerSec), m_visionRadius(visionRadius) {
    SetupStates();
    m_puddleTimer.Start(kPuddleInterval); // para que un Trapper no suelte charco al nacer

    m_model = LoadModel("assets/models/enemy/character_l.glb");

    m_hurtSound = LoadSound("assets/audio/sfx/attach_zombie.wav");
    m_deathSound = LoadSound("assets/audio/sfx/dead_zombie.ogg");
    RefreshSfxVolume();
}

void Enemy::RefreshSfxVolume() {
    float sfxVolume = AudioSettings::GetSfxVolume();
    if (m_hurtSound.frameCount > 0) SetSoundVolume(m_hurtSound, sfxVolume);
    if (m_deathSound.frameCount > 0) SetSoundVolume(m_deathSound, sfxVolume);
}

Enemy::~Enemy() {
    ModelUtils::UnloadModelAndTextures(m_model);
    if (m_hurtSound.frameCount > 0) UnloadSound(m_hurtSound);
    if (m_deathSound.frameCount > 0) UnloadSound(m_deathSound);
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

    StateMachine<EnemyState>::StateCallbacks attackRanged;
    attackRanged.onUpdate = [this](float dt) { UpdateAttackRanged(dt); };
    m_fsm.RegisterState(EnemyState::AttackRanged, attackRanged);

    StateMachine<EnemyState>::StateCallbacks explode;
    explode.onEnter = [this]() { EnterExplode(); };
    explode.onUpdate = [this](float dt) { UpdateExplode(dt); };
    m_fsm.RegisterState(EnemyState::Explode, explode);

    StateMachine<EnemyState>::StateCallbacks hurt;
    hurt.onEnter = [this]() {
        m_hurtTimer = 0.0f;
        if (m_hurtSound.frameCount > 0) PlaySound(m_hurtSound);
    };
    hurt.onUpdate = [this](float dt) { UpdateHurt(dt); };
    m_fsm.RegisterState(EnemyState::Hurt, hurt);

    StateMachine<EnemyState>::StateCallbacks dead;
    dead.onEnter = [this]() { EnterDead(); };
    dead.onUpdate = [this](float dt) { UpdateDead(dt); };
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

    FaceTowards(dir, dt);

    float speed = CurrentSpeed();
    TryMoveAgainstObstacles(Vector3{ dir.x * speed * dt, 0.0f, dir.z * speed * dt });
}

void Enemy::UpdateChase(float dt) {
    float distSq = CollisionMath::DistanceSquared(m_position, m_lastKnownPlayerPosition);

    // Qué hace al alcanzar al jugador depende de su comportamiento.
    if (m_playerVisible) {
        switch (m_behavior) {
            case EnemyBehavior::Kamikaze:
                if (distSq <= kKamikazeExplodeTriggerRange * kKamikazeExplodeTriggerRange) {
                    m_fsm.ChangeState(EnemyState::Explode);
                    return;
                }
                break;
            case EnemyBehavior::Spitter:
                if (distSq <= kRangedAttackRange * kRangedAttackRange) {
                    m_fsm.ChangeState(EnemyState::AttackRanged);
                    return;
                }
                break;
            case EnemyBehavior::Buffer:
                // Se planta mirando al jugador y deja que su aura acelere al resto.
                if (distSq <= kBufferKeepDistance * kBufferKeepDistance) {
                    FaceTowards(DirectionToLastKnownPlayer(), dt);
                    return;
                }
                break;
            case EnemyBehavior::Melee:
            case EnemyBehavior::Shielder:
            case EnemyBehavior::Trapper:
                if (distSq <= kAttackRange * kAttackRange) {
                    m_fsm.ChangeState(EnemyState::Attack);
                    return;
                }
                break;
        }
    }

    // Si pierde de vista al jugador y llega al último punto visto, vuelve a patrullar.
    constexpr float kGiveUpThreshold = 0.1f;
    if (!m_playerVisible && distSq <= kGiveUpThreshold * kGiveUpThreshold) {
        m_fsm.ChangeState(EnemyState::Patrol);
        return;
    }

    // Se mueve en línea recta hacia el jugador, pero gira con el límite de su arquetipo.
    Vector3 dir = DirectionToLastKnownPlayer();
    FaceTowards(dir, dt);

    float speed = CurrentSpeed();
    TryMoveAgainstObstacles(Vector3{ dir.x * speed * dt, 0.0f, dir.z * speed * dt });
}

void Enemy::FaceTowards(Vector3 targetDirection, float dt) {
    if (targetDirection.x == 0.0f && targetDirection.z == 0.0f) return;

    if (m_turnRateDegPerSec <= 0.0f) { // 0 = giro instantáneo
        m_facingDirection = targetDirection;
        return;
    }

    float currentAngle = atan2f(m_facingDirection.x, m_facingDirection.z);
    float targetAngle = atan2f(targetDirection.x, targetDirection.z);

    // Normaliza la diferencia de ángulo para girar siempre por el camino más corto.
    float delta = targetAngle - currentAngle;
    while (delta > PI) delta -= 2.0f * PI;
    while (delta < -PI) delta += 2.0f * PI;

    float maxStep = m_turnRateDegPerSec * (PI / 180.0f) * dt;
    if (delta > maxStep) delta = maxStep;
    if (delta < -maxStep) delta = -maxStep;

    float newAngle = currentAngle + delta;
    m_facingDirection = Vector3{ sinf(newAngle), 0.0f, cosf(newAngle) };
}

Vector3 Enemy::DirectionToLastKnownPlayer() const {
    return CollisionMath::DirectionXZ(m_position, m_lastKnownPlayerPosition);
}

void Enemy::EnterAttack() {
}

Hitbox Enemy::SpawnAttackHitbox() const {
    constexpr float kKnockbackForce = 9.0f;
    // El golpe sale en la dirección hacia la que mira el enemigo.
    return CombatSystem::BuildMeleeHitbox(m_position, m_facingDirection, m_attackDamage, kKnockbackForce);
}

void Enemy::UpdateAttack(float dt) {
    FaceTowards(DirectionToLastKnownPlayer(), dt);

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

void Enemy::UpdateAttackRanged(float dt) {
    // Mantiene las distancias: solo gira hacia el jugador y dispara, sin moverse.
    FaceTowards(DirectionToLastKnownPlayer(), dt);

    m_attackCooldown -= dt;
    if (m_attackCooldown <= 0.0f) {
        m_attackCooldown = kRangedAttackInterval;

        Projectile projectile;
        projectile.position = Vector3{ m_position.x, m_position.y + 0.5f, m_position.z };
        projectile.velocity = Vector3{
            m_facingDirection.x * kProjectileSpeed, 0.0f, m_facingDirection.z * kProjectileSpeed
        };
        projectile.damage = m_attackDamage;
        m_queuedProjectile = projectile;
        m_pendingProjectile = true;
    }

    // Margen extra antes de soltar el estado, para no parpadear en el borde del rango.
    constexpr float kRangedGiveUpMultiplier = 1.3f;
    float giveUpRange = kRangedAttackRange * kRangedGiveUpMultiplier;
    float distSq = CollisionMath::DistanceSquared(m_position, m_lastKnownPlayerPosition);
    if (!m_playerVisible || distSq > giveUpRange * giveUpRange) {
        m_fsm.ChangeState(EnemyState::Chase);
    }
}

void Enemy::EnterExplode() {
    m_explodeTimer = 0.0f;
}

void Enemy::UpdateExplode(float dt) {
    m_explodeTimer += dt;
    if (m_explodeTimer >= kExplodeDuration) {
        m_pendingExplosion = true;
        TakeDamage(m_maxHP, Vector3{ 0.0f, 0.0f, 0.0f }); // se mata con su propio daño
    }
}

void Enemy::UpdateHurt(float dt) {
    m_hurtTimer += dt;
    if (m_hurtTimer >= kHurtDuration) {
        m_fsm.ChangeState(EnemyState::Chase);
    }
}

void Enemy::EnterDead() {
    std::cout << "[Combate] Zombie derrotado." << std::endl;
    m_deathTimer = 0.0f;
    if (m_deathSound.frameCount > 0) PlaySound(m_deathSound);
}

void Enemy::UpdateDead(float dt) {
    m_deathTimer += dt;
    if (m_deathTimer >= kCorpseFadeDuration) {
        m_pendingDestruction = true;
    }
}

void Enemy::Update(float dt) {
    m_damageFlashTimer.Tick(dt);

    // Trapper: gotea un charco cada cierto tiempo, en cualquier estado.
    if (m_behavior == EnemyBehavior::Trapper && IsAlive()) {
        m_puddleTimer.Tick(dt);
        if (!m_puddleTimer.IsActive()) {
            m_puddleTimer.Start(kPuddleInterval);
            m_queuedPuddlePosition = m_position;
            m_pendingPuddle = true;
        }
    }

    ApplyKnockback(dt);
    m_fsm.Update(dt);
}

bool Enemy::BlocksAttackFrom(Vector3 attackerPosition) const {
    if (m_behavior != EnemyBehavior::Shielder) return false;

    Vector3 toAttacker = CollisionMath::DirectionXZ(m_position, attackerPosition);
    float facingDot = toAttacker.x * m_facingDirection.x + toAttacker.z * m_facingDirection.z;
    return facingDot >= kShieldBlockCosine;
}

void Enemy::Draw() const {
    float rotationAngle = CollisionMath::HeadingDegrees(m_facingDirection);

    Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };
    Vector3 scale = { m_scale, m_scale, m_scale };

    Color tint = m_baseTint;
    unsigned char shadowAlpha = 100;
    if (m_fsm.Is(EnemyState::Hurt)) {
        tint = RED;
    } else if (m_fsm.Is(EnemyState::Dead)) {
        // El cadáver se desvanece y parpadea a la vez mientras dura el fade-out.
        constexpr Color kDeadTint = Color{ 80, 20, 20, 255 };
        constexpr float kCorpseFlickerPeriod = 0.2f;

        float alphaFloat = 1.0f - Pulse::Progress01(m_deathTimer, kCorpseFadeDuration);
        unsigned char alpha = (unsigned char)(alphaFloat * 255.0f);

        tint = Pulse::Blink(m_deathTimer, kCorpseFlickerPeriod) ? WHITE : kDeadTint;
        tint.a = alpha;
        shadowAlpha = (unsigned char)(alphaFloat * 100.0f);
    } else if (m_fsm.Is(EnemyState::Explode)) {
        // Parpadeo que se acelera a medida que se acerca la detonación.
        tint = Pulse::AcceleratingBlink(m_explodeTimer, Pulse::Progress01(m_explodeTimer, kExplodeDuration), 0.3f, 0.05f)
                   ? WHITE : RED;
    }

    // Destello breve al recibir daño, sin tocar el alpha (para no romper el fade del cadáver).
    if (m_damageFlashTimer.IsActive()) {
        tint.r = 255;
        tint.g = 255;
        tint.b = 255;
    }

    // Con el cadáver semitransparente, se desactiva la escritura de profundidad
    // para que no recorte lo que se dibuje detrás.
    bool isFading = tint.a < 255;
    if (isFading) rlDisableDepthMask();

    // Sombra falsa en el suelo.
    DrawCylinder(Vector3{ m_position.x, 0.01f, m_position.z }, 0.6f * m_scale, 0.6f * m_scale, 0.01f, 15, Color{ 0, 0, 0, shadowAlpha });

    // Dibuja el cuerpo con un contorno estilo anime.
    ModelUtils::DrawModelWithOutline(m_model, m_position, rotationAxis, rotationAngle, scale, tint);

    DrawArchetypeDecoration(rotationAngle, tint);

    if (isFading) rlEnableDepthMask();
}

void Enemy::DrawArchetypeDecoration(float rotationAngleDegrees, Color tint) const {
    // El aro del aura del Buffer no gira con el cuerpo y desaparece al morir.
    if (m_behavior == EnemyBehavior::Buffer) {
        if (!IsAlive()) return;

        float pulse = Pulse::Wave01(static_cast<float>(GetTime()), 3.0f);
        float radius = kBufferAuraRadius * (0.96f + 0.04f * pulse);
        Vector3 base{ m_position.x, 0.03f, m_position.z };
        DrawCylinder(base, radius, radius, 0.02f, 32, Fade(GOLD, 0.06f + 0.08f * pulse));
        DrawCylinderWires(base, radius, radius, 0.02f, 32, Fade(GOLD, 0.45f + 0.4f * pulse));
        return;
    }

    if (m_behavior != EnemyBehavior::Shielder && m_behavior != EnemyBehavior::Trapper) return;

    // Rota la matriz de mundo para que el adorno gire junto con el cuerpo.
    rlPushMatrix();
    rlTranslatef(m_position.x, m_position.y, m_position.z);
    rlRotatef(rotationAngleDegrees, 0.0f, 1.0f, 0.0f);

    // El adorno hereda el alpha del cuerpo, para desvanecerse igual durante el fade.
    float alpha = tint.a / 255.0f;

    if (m_behavior == EnemyBehavior::Shielder) {
        // Placa frontal, en el mismo cono que bloquea los golpes.
        Vector3 plateCenter{ 0.0f, 0.55f * m_scale, 0.6f * m_scale };
        DrawCube(plateCenter, 1.2f * m_scale, 1.3f * m_scale, 0.16f * m_scale, tint);
        DrawCubeWires(plateCenter, 1.2f * m_scale, 1.3f * m_scale, 0.16f * m_scale, Fade(SKYBLUE, alpha));
    } else {
        // Depósito de ácido a la espalda, de donde salen los charcos.
        Vector3 tankCenter{ 0.0f, 0.9f * m_scale, -0.5f * m_scale };
        DrawSphere(tankCenter, 0.34f * m_scale, Fade(Color{ 70, 210, 70, 255 }, alpha));
        DrawSphereWires(tankCenter, 0.36f * m_scale, 6, 8, Fade(DARKGREEN, alpha));
    }

    rlPopMatrix();
}

void Enemy::TakeDamage(float amount, Vector3 knockbackDir) {
    if (!IsAlive()) return;
    Actor::TakeDamage(amount, knockbackDir);
    m_damageFlashTimer.Start(kDamageFlashDuration);
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

bool Enemy::ConsumePendingProjectile(Projectile& outProjectile) {
    if (!m_pendingProjectile) return false;
    outProjectile = m_queuedProjectile;
    m_pendingProjectile = false;
    return true;
}

bool Enemy::ConsumeExplosionTrigger() {
    if (!m_pendingExplosion) return false;
    m_pendingExplosion = false;
    return true;
}

bool Enemy::ConsumePendingPuddle(Vector3& outPosition) {
    if (!m_pendingPuddle) return false;
    outPosition = m_queuedPuddlePosition;
    m_pendingPuddle = false;
    return true;
}

void Enemy::SetShader(Shader shader) {
    ModelUtils::ApplyShaderToMaterials(m_model, shader);
}
