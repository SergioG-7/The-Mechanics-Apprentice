#include "Enemy.h"
#include "../Combat/CollisionMath.h"
#include "../Combat/CombatSystem.h"
#include "../Core/AudioSettings.h"
#include "../Renderer/ModelUtils.h"
#include "raylib.h"
#include "rlgl.h"
#include <iostream>

Enemy::Enemy(Vector3 position, float maxHP, std::vector<Vector3> patrolRoute, float visionRadius,
             float speed, float attackDamage, float scale, EnemyBehavior behavior)
    : Actor(position, maxHP, Vector3{ 0.5f * scale, 0.5f * scale, 0.5f * scale }),
      m_patrolRoute(std::move(patrolRoute)), m_speed(speed),
      m_attackDamage(attackDamage), m_scale(scale), m_behavior(behavior), m_visionRadius(visionRadius) {
    SetupStates();

    // Modelo Kenney (blocky-characters, variante L / "zombie"): mismo caso
    // que el Player, trae su atlas vía baseColorTexture y raylib lo resuelve
    // solo. El color base del material ya nace en blanco.
    m_model = LoadModel("assets/models/enemy/character_l.glb");

    // "hurt.ogg" (de una sesión anterior) no existe en disco -- LoadSound
    // fallaba en silencio (frameCount 0) y este sonido nunca llegaba a
    // sonar. "attach_zombie.wav" es el archivo real de reacción al daño del
    // zombie (nombre tal cual está en assets/audio/sfx/).
    m_hurtSound = LoadSound("assets/audio/sfx/attach_zombie.wav");
    m_deathSound = LoadSound("assets/audio/sfx/dead_zombie.ogg");
    RefreshSfxVolume();
}

void Enemy::RefreshSfxVolume() {
    // Directo, sin multiplicador propio -- ver Player::RefreshSfxVolume,
    // mismo motivo: al 100% de SFX, SetSoundVolume tiene que recibir 1.0.
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

    m_facingDirection = dir;

    TryMoveAgainstObstacles(Vector3{ dir.x * m_speed * dt, 0.0f, dir.z * m_speed * dt });
}

void Enemy::UpdateChase(float dt) {
    float distSq = CollisionMath::DistanceSquared(m_position, m_lastKnownPlayerPosition);

    // A qué estado transiciona al alcanzar al jugador depende del
    // comportamiento (ver EnemyBehavior): un Kamikaze detona, un Spitter
    // dispara a distancia, y el resto (Melee, por defecto) ataca cuerpo a
    // cuerpo como siempre.
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
            case EnemyBehavior::Melee:
                if (distSq <= kAttackRange * kAttackRange) {
                    m_fsm.ChangeState(EnemyState::Attack);
                    return;
                }
                break;
        }
    }

    // El jugador salió de rango de visión: m_lastKnownPlayerPosition queda
    // congelada en el último punto visto. Al llegar ahí sin recuperar
    // visibilidad, no queda rastro que seguir -- volver a patrullar en vez
    // de quedarse plantado (o, antes de este fix, reentrar en Attack contra
    // una posición vacía y no volver a moverse nunca).
    constexpr float kGiveUpThreshold = 0.1f;
    if (!m_playerVisible && distSq <= kGiveUpThreshold * kGiveUpThreshold) {
        m_fsm.ChangeState(EnemyState::Patrol);
        return;
    }

    Vector3 dir = DirectionToLastKnownPlayer();

    m_facingDirection = dir;

    TryMoveAgainstObstacles(Vector3{ dir.x * m_speed * dt, 0.0f, dir.z * m_speed * dt });
}

Vector3 Enemy::DirectionToLastKnownPlayer() const {
    Vector3 toPlayer{
        m_lastKnownPlayerPosition.x - m_position.x, 0.0f,
        m_lastKnownPlayerPosition.z - m_position.z
    };
    return CollisionMath::Normalize2D(toPlayer);
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
    return CombatSystem::BuildMeleeHitbox(m_position, DirectionToLastKnownPlayer(), m_attackDamage, kKnockbackForce);
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

void Enemy::UpdateAttackRanged(float dt) {
    // Mantiene las distancias: a diferencia de UpdateAttack, nunca llama a
    // TryMoveAgainstObstacles -- solo gira hacia el jugador y dispara. No
    // retrocede si el jugador se acerca demasiado (simplificación deliberada:
    // un Spitter arrinconado sigue disparando en vez de huir).
    m_facingDirection = DirectionToLastKnownPlayer();

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

    // Histéresis (rango real x1.3 para soltar el estado): sin ella, un
    // jugador parado justo en el borde de kRangedAttackRange haría que el
    // Spitter parpadeara entre Chase y AttackRanged cada frame.
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
    // No se mueve durante la cuenta atrás -- por diseño, sin llamada a
    // TryMoveAgainstObstacles.
    m_explodeTimer += dt;
    if (m_explodeTimer >= kExplodeDuration) {
        m_pendingExplosion = true;
        // Se mata con su propio daño: reutiliza toda la ruta normal de
        // muerte (Hurt/Dead, fade, corpse cleanup) en vez de duplicarla.
        TakeDamage(m_maxHP, Vector3{ 0.0f, 0.0f, 0.0f });
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
    ApplyKnockback(dt);
    m_fsm.Update(dt);
}

void Enemy::Draw() const {
    float rotationAngle = 0.0f;
    if (m_facingDirection.x != 0.0f || m_facingDirection.z != 0.0f) {
        rotationAngle = atan2f(m_facingDirection.x, m_facingDirection.z) * (180.0f / PI);
    }

    Vector3 rotationAxis = { 0.0f, 1.0f, 0.0f };
    Vector3 scale = { m_scale, m_scale, m_scale };

    // WHITE en normal (igual que el Player) para no teñir el material sobre
    // el que trabaja el toon shader; el flash de daño pasa a RED, ya que
    // WHITE dejaría de contrastar contra un tinte neutro. Tono sangre seca
    // cuando cae derrotado.
    Color tint = WHITE;
    unsigned char shadowAlpha = 100;
    if (m_fsm.Is(EnemyState::Hurt)) {
        tint = RED;
    } else if (m_fsm.Is(EnemyState::Dead)) {
        // Sin pausa: el fade-out arranca en el instante mismo de la muerte
        // (m_deathTimer = 0) y dura kCorpseFadeDuration completo, sin un
        // tramo previo opaco. Mientras el alpha baja, el color parpadea a
        // velocidad constante (no acelerada, a diferencia de Explode más
        // abajo) entre WHITE puro -- un flash tipo stun -- y el tinte de
        // muerte de siempre; ambos frames comparten el MISMO alpha del
        // instante, así que el parpadeo no interfiere con el fade.
        constexpr Color kDeadTint = Color{ 80, 20, 20, 255 };
        constexpr float kCorpseFlickerPeriod = 0.2f;

        float alphaFloat = 1.0f - (m_deathTimer / kCorpseFadeDuration);
        if (alphaFloat < 0.0f) alphaFloat = 0.0f;
        unsigned char alpha = (unsigned char)(alphaFloat * 255.0f);

        bool flickerOn = fmodf(m_deathTimer, kCorpseFlickerPeriod) < (kCorpseFlickerPeriod * 0.5f);
        tint = flickerOn ? WHITE : kDeadTint;
        tint.a = alpha;
        shadowAlpha = (unsigned char)(alphaFloat * 100.0f);
    } else if (m_fsm.Is(EnemyState::Explode)) {
        // Parpadeo cada vez más rápido: el período baja de 0.3s a 0.05s a
        // medida que se acerca la detonación (progress 0..1).
        float progress = m_explodeTimer / kExplodeDuration;
        float period = 0.3f - 0.25f * progress;
        if (period < 0.02f) period = 0.02f; // suelo: evita un fmodf casi degenerado
        bool flashOn = fmodf(m_explodeTimer, period) < (period * 0.5f);
        tint = flashOn ? WHITE : RED;
    }

    // Hit-flash: destello breve de impacto, independiente del tinte de
    // estado de arriba (incluida la propia muerte) -- por eso se aplica el
    // último y sin "else", manda sobre cualquier cosa durante sus 0.1s. Solo
    // se tocan los canales RGB: si esto reescribiera tint entero (WHITE
    // incluye alpha 255), pisaría el alpha del fade de muerte de arriba y el
    // cadáver "resucitaría" a opaco durante ese destello.
    if (m_damageFlashTimer.IsActive()) {
        tint.r = 255;
        tint.g = 255;
        tint.b = 255;
    }

    // Mientras el cadáver se desvanece (alpha < 255), el depth write se
    // desactiva: si no, cada píxel semitransparente sigue escribiendo el
    // depth buffer como si fuera opaco, y cualquier cosa dibujada después
    // detrás/alrededor del cadáver (otro enemigo, una partícula) se recorta
    // contra un cuerpo que ya casi no se ve -- el "parpadeo" reportado. El
    // depth TEST se deja activo (rlDisableDepthMask, no rlDisableDepthTest):
    // el cadáver sigue ocultándose correctamente detrás de un Obstacle real.
    bool isFading = tint.a < 255;
    if (isFading) rlDisableDepthMask();

    // Sombra falsa: ancla al zombie al suelo sin necesitar un shader de
    // sombras real. Y a 0.01f para evitar z-fighting con el suelo.
    DrawCylinder(Vector3{ m_position.x, 0.01f, m_position.z }, 0.6f * m_scale, 0.6f * m_scale, 0.01f, 15, Color{ 0, 0, 0, shadowAlpha });

    // Outline estilo anime ("inverted hull"), igual que Player::Draw -- ver
    // ModelUtils::DrawModelWithOutline. Negro puro con el mismo alpha que el
    // cuerpo para que se desvanezca a la par durante el fade del cadáver.
    ModelUtils::DrawModelWithOutline(m_model, m_position, rotationAxis, rotationAngle, scale, tint);

    if (isFading) rlEnableDepthMask();
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

void Enemy::SetShader(Shader shader) {
    ModelUtils::ApplyShaderToMaterials(m_model, shader);
}
