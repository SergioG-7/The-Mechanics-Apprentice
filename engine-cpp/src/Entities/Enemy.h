#pragma once
#include "Actor.h"
#include "../Core/FSM/StateMachine.h"
#include "../Core/CountdownTimer.h"
#include "../Combat/Hitbox.h"
#include "../Combat/Projectile.h"
#include <vector>

enum class EnemyState { Patrol, Chase, Attack, AttackRanged, Explode, Hurt, Dead };

// Comportamiento de combate del enemigo: cómo ataca al alcanzar al jugador.
//   Shielder: bloquea los golpes que recibe de frente.
//   Buffer:   no ataca, acelera a los enemigos cercanos.
//   Trapper:  melee normal, pero deja charcos de lodo al pasar.
enum class EnemyBehavior { Melee, Kamikaze, Spitter, Shielder, Buffer, Trapper };

class Enemy : public Actor {
public:
    Enemy(Vector3 position, float maxHP, std::vector<Vector3> patrolRoute, float visionRadius,
          float speed, float attackDamage, float scale = 1.0f, EnemyBehavior behavior = EnemyBehavior::Melee,
          Color baseTint = WHITE, float turnRateDegPerSec = 0.0f);
    ~Enemy();
    void Update(float dt) override;
    void Draw() const override;
    void TakeDamage(float amount, Vector3 knockbackDir) override;
    void NotifyPlayerPosition(Vector3 playerPosition);

    // Hitbox activa de este frame, o nullptr si no está atacando.
    const Hitbox* GetActiveHitbox() const;
    void CloseAttackHitbox();

    // Asigna el shader al modelo del cuerpo.
    void SetShader(Shader shader);

    // Actualiza el volumen de los sonidos según los ajustes de audio actuales.
    void RefreshSfxVolume();

    // true cuando el cadáver ya ha terminado de desvanecerse y se puede eliminar.
    bool IsPendingDestruction() const { return m_pendingDestruction; }

    // Devuelven true la primera vez que hay un proyectil o explosión pendiente por generar.
    bool ConsumePendingProjectile(Projectile& outProjectile);
    bool ConsumeExplosionTrigger();

    // Devuelve true cuando toca dejar un nuevo charco de lodo (Trapper).
    bool ConsumePendingPuddle(Vector3& outPosition);

    float GetExplosionDamage() const { return m_attackDamage; }
    static constexpr float kExplodeRadius = 3.0f; // radio de la explosión del Kamikaze

    EnemyBehavior GetBehavior() const { return m_behavior; }

    // true si es un Shielder y el ataque le llega por el frente protegido.
    bool BlocksAttackFrom(Vector3 attackerPosition) const;

    // Multiplicador de velocidad aplicado por el aura de un Buffer cercano.
    void SetSpeedMultiplier(float multiplier) { m_speedMultiplier = multiplier; }

    static constexpr float kBufferAuraRadius = 6.0f;
    static constexpr float kBufferSpeedBonus = 1.4f;

private:
    void SetupStates();
    void UpdatePatrol(float dt);
    void UpdateChase(float dt);
    void EnterAttack();
    void UpdateAttack(float dt);
    void UpdateAttackRanged(float dt);
    void EnterExplode();
    void UpdateExplode(float dt);
    void UpdateHurt(float dt);
    void EnterDead();
    void UpdateDead(float dt);
    Hitbox SpawnAttackHitbox() const;

    // Dirección normalizada hacia la última posición conocida del jugador.
    Vector3 DirectionToLastKnownPlayer() const;

    // Velocidad efectiva de este frame (base multiplicada por el aura de un Buffer).
    float CurrentSpeed() const { return m_speed * m_speedMultiplier; }

    // Gira la dirección de encaramiento hacia el objetivo, respetando el límite de giro.
    void FaceTowards(Vector3 targetDirection, float dt);

    // Dibuja la geometría extra que distingue a cada arquetipo (placa, aura, depósito...).
    void DrawArchetypeDecoration(float rotationAngleDegrees, Color tint) const;

    StateMachine<EnemyState> m_fsm;
    Vector3 m_facingDirection = { 0.0f, 0.0f, 1.0f };
    std::vector<Vector3> m_patrolRoute;
    size_t m_currentPatrolIndex = 0;

    float m_speed;
    float m_attackDamage;
    float m_scale = 1.0f;
    EnemyBehavior m_behavior = EnemyBehavior::Melee;
    Color m_baseTint = WHITE;
    float m_speedMultiplier = 1.0f;
    float m_turnRateDegPerSec = 0.0f;

    float m_visionRadius;
    Vector3 m_lastKnownPlayerPosition{};
    bool m_playerVisible = false;

    float m_attackCooldown = 0.0f;
    float m_hurtTimer = 0.0f;
    static constexpr float kAttackRange = 1.2f;
    static constexpr float kAttackInterval = 1.0f;
    static constexpr float kHurtDuration = 0.6f;

    // Destello breve al recibir daño.
    CountdownTimer m_damageFlashTimer;
    static constexpr float kDamageFlashDuration = 0.1f;

    // Spitter: mantiene las distancias y dispara en vez de acercarse.
    static constexpr float kRangedAttackRange = 8.0f;
    static constexpr float kRangedAttackInterval = 1.5f;
    static constexpr float kProjectileSpeed = 9.0f;
    bool m_pendingProjectile = false;
    Projectile m_queuedProjectile{};

    // Kamikaze: deja de moverse, parpadea y detona tras este tiempo.
    static constexpr float kKamikazeExplodeTriggerRange = 2.0f;
    static constexpr float kExplodeDuration = 1.5f;
    float m_explodeTimer = 0.0f;
    bool m_pendingExplosion = false;

    // Shielder: ángulo del cono frontal que bloquea los golpes.
    static constexpr float kShieldBlockCosine = 0.5f;

    // Buffer: se queda a esta distancia del jugador en vez de acercarse.
    static constexpr float kBufferKeepDistance = 5.0f;

    // Trapper: deja un charco cada este intervalo.
    static constexpr float kPuddleInterval = 2.5f;
    CountdownTimer m_puddleTimer;
    bool m_pendingPuddle = false;
    Vector3 m_queuedPuddlePosition{};

    // Fade-out del cadáver antes de eliminarlo.
    float m_deathTimer = 0.0f;
    bool m_pendingDestruction = false;
    static constexpr float kCorpseFadeDuration = 2.0f;

    Hitbox m_activeHitbox{};
    bool m_hitboxWindowOpen = false;
    Model m_model;
    Sound m_hurtSound{};
    Sound m_deathSound{};
};
