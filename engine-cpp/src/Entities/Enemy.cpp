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

    // Arrancado, no en cero: si no, un Trapper soltaría su primer charco en
    // el frame mismo en que nace, encima de su propio spawn.
    m_puddleTimer.Start(kPuddleInterval);

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

    FaceTowards(dir, dt);

    float speed = CurrentSpeed();
    TryMoveAgainstObstacles(Vector3{ dir.x * speed * dt, 0.0f, dir.z * speed * dt });
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
            case EnemyBehavior::Buffer:
                // No ataca nunca: al entrar en su radio de mando se planta,
                // mirando al jugador, y deja que su aura empuje a los demás.
                if (distSq <= kBufferKeepDistance * kBufferKeepDistance) {
                    FaceTowards(DirectionToLastKnownPlayer(), dt);
                    return;
                }
                break;
            case EnemyBehavior::Melee:
            case EnemyBehavior::Shielder:
            case EnemyBehavior::Trapper:
                // Los tres cierran a melee igual; lo que los diferencia
                // (placa frontal, goteo de lodo) no toca la FSM.
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

    // Se PERSIGUE en línea recta pero se GIRA con el límite del arquetipo:
    // ahí está el hueco del Shielder. Sigue viniendo a por ti, pero su placa
    // tarda en reorientarse, así que un Dash a su espalda llega antes que
    // ella. Si el movimiento usara el encaramiento en vez de dir, se quedaría
    // dando vueltas contra las paredes en lugar de resultar esquivable.
    FaceTowards(dir, dt);

    float speed = CurrentSpeed();
    TryMoveAgainstObstacles(Vector3{ dir.x * speed * dt, 0.0f, dir.z * speed * dt });
}

void Enemy::FaceTowards(Vector3 targetDirection, float dt) {
    if (targetDirection.x == 0.0f && targetDirection.z == 0.0f) return;

    // 0 = sin límite: el comportamiento de siempre para todo arquetipo que no
    // sea el Shielder, y el que hay que preservar exactamente (un Melee que
    // empezara a girar despacio dejaría de conectar sus golpes).
    if (m_turnRateDegPerSec <= 0.0f) {
        m_facingDirection = targetDirection;
        return;
    }

    float currentAngle = atan2f(m_facingDirection.x, m_facingDirection.z);
    float targetAngle = atan2f(targetDirection.x, targetDirection.z);

    // Diferencia normalizada a [-PI, PI]: sin esto, girar de +170° a -170°
    // (20° reales) daría una vuelta de 340° por el lado largo.
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
    // No resetear m_attackCooldown aquí: si el jugador oscila en el borde de
    // kAttackRange, el FSM re-entra en Attack varias veces por segundo y un
    // reset a 0 dispararía un golpe instantáneo cada vez, saltándose kAttackInterval.
}

Hitbox Enemy::SpawnAttackHitbox() const {
    // Subido de 6.0f: da al jugador más distancia tras un golpe, con margen
    // real para reaccionar con el Dash en vez de quedar pegado al enemigo.
    constexpr float kKnockbackForce = 9.0f;
    // m_facingDirection, no DirectionToLastKnownPlayer(): el golpe sale por
    // donde el enemigo MIRA. Para todo arquetipo de giro instantáneo son el
    // mismo vector (UpdateAttack acaba de encararlo), pero para el Shielder
    // significa que su ataque también se queda atrás al rodearlo, en vez de
    // seguir apuntando al jugador con el cuerpo girado.
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
    // Mantiene las distancias: a diferencia de UpdateAttack, nunca llama a
    // TryMoveAgainstObstacles -- solo gira hacia el jugador y dispara. No
    // retrocede si el jugador se acerca demasiado (simplificación deliberada:
    // un Spitter arrinconado sigue disparando en vez de huir).
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

    // Trapper: gotea un charco cada kPuddleInterval mientras siga vivo, sea
    // cual sea su estado -- también patrullando, para que el nivel se vaya
    // ensuciando aunque el jugador todavía no lo haya visto.
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

    // Código de color del arquetipo (WHITE para los que no declaran ninguno,
    // igual que antes: no tiñe el material sobre el que trabaja el toon
    // shader). El flash de daño pasa a RED, ya que WHITE dejaría de
    // contrastar contra un tinte neutro. Tono sangre seca cuando cae
    // derrotado, por encima del tinte de arquetipo.
    Color tint = m_baseTint;
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

        float alphaFloat = 1.0f - Pulse::Progress01(m_deathTimer, kCorpseFadeDuration);
        unsigned char alpha = (unsigned char)(alphaFloat * 255.0f);

        tint = Pulse::Blink(m_deathTimer, kCorpseFlickerPeriod) ? WHITE : kDeadTint;
        tint.a = alpha;
        shadowAlpha = (unsigned char)(alphaFloat * 100.0f);
    } else if (m_fsm.Is(EnemyState::Explode)) {
        // Parpadeo cada vez más rápido: el período baja de 0.3s a 0.05s a
        // medida que se acerca la detonación.
        tint = Pulse::AcceleratingBlink(m_explodeTimer, Pulse::Progress01(m_explodeTimer, kExplodeDuration), 0.3f, 0.05f)
                   ? WHITE : RED;
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

    DrawArchetypeDecoration(rotationAngle, tint);

    if (isFading) rlEnableDepthMask();
}

void Enemy::DrawArchetypeDecoration(float rotationAngleDegrees, Color tint) const {
    // El aro del Buffer va en coordenadas de mundo (no gira con el cuerpo) y
    // se apaga con el cadáver, así que sale de la matriz rotada de abajo.
    if (m_behavior == EnemyBehavior::Buffer) {
        if (!IsAlive()) return;

        // Pulso lento: el radio y la opacidad respiran a la vez, para que se
        // lea como un área de influencia activa y no como decoración fija.
        float pulse = Pulse::Wave01(static_cast<float>(GetTime()), 3.0f);
        float radius = kBufferAuraRadius * (0.96f + 0.04f * pulse);
        Vector3 base{ m_position.x, 0.03f, m_position.z };
        DrawCylinder(base, radius, radius, 0.02f, 32, Fade(GOLD, 0.06f + 0.08f * pulse));
        DrawCylinderWires(base, radius, radius, 0.02f, 32, Fade(GOLD, 0.45f + 0.4f * pulse));
        return;
    }

    if (m_behavior != EnemyBehavior::Shielder && m_behavior != EnemyBehavior::Trapper) return;

    // Mismo truco que Gear/PowerUp: DrawCube/DrawSphere no aceptan ángulo,
    // así que se rota la matriz de mundo con el MISMO ángulo que el cuerpo
    // -- así la placa sigue mirando adelante y el depósito sigue a la
    // espalda cuando el enemigo gira.
    rlPushMatrix();
    rlTranslatef(m_position.x, m_position.y, m_position.z);
    rlRotatef(rotationAngleDegrees, 0.0f, 1.0f, 0.0f);

    // Los adornos heredan el alpha del cuerpo: durante el fade del cadáver,
    // una placa o un depósito opacos se quedarían flotando sobre un zombie
    // que ya casi no se ve.
    float alpha = tint.a / 255.0f;

    if (m_behavior == EnemyBehavior::Shielder) {
        // Placa frontal: cubre justo el cono que BlocksAttackFrom protege,
        // así que lo que se ve es literalmente por dónde NO entra el golpe.
        Vector3 plateCenter{ 0.0f, 0.55f * m_scale, 0.6f * m_scale };
        DrawCube(plateCenter, 1.2f * m_scale, 1.3f * m_scale, 0.16f * m_scale, tint);
        DrawCubeWires(plateCenter, 1.2f * m_scale, 1.3f * m_scale, 0.16f * m_scale, Fade(SKYBLUE, alpha));
    } else {
        // Depósito de ácido a la espalda: de ahí salen los charcos.
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
