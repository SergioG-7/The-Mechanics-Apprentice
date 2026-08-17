#pragma once
#include "Actor.h"
#include "../Core/FSM/StateMachine.h"
#include "../Combat/Hitbox.h"
#include "../Combat/Projectile.h"
#include <vector>

enum class EnemyState { Patrol, Chase, Attack, AttackRanged, Explode, Hurt, Dead };

// Perfil de combate del enemigo, resuelto por EnemyFactory a partir del
// campo "behavior" de enemy_variants.json (ver EnemyFactory.cpp). Determina
// a qué estado transiciona UpdateChase al alcanzar al jugador -- el resto de
// la FSM (Patrol/Hurt/Dead) es idéntico para los tres.
enum class EnemyBehavior { Melee, Kamikaze, Spitter };

class Enemy : public Actor {
public:
    // scale multiplica tanto el modelo dibujado como el halfExtents de
    // colisión (ver constructor); por defecto 1.0f para no afectar a los
    // enemigos que ya construye LevelLoader desde el JSON de nivel. behavior
    // por defecto Melee, por la misma razón: un enemigo "Default" del JSON
    // de nivel no pasa por EnemyFactory y no debe comportarse como Spitter o
    // Kamikaze sin que nadie se lo haya pedido explícitamente.
    Enemy(Vector3 position, float maxHP, std::vector<Vector3> patrolRoute, float visionRadius,
          float speed, float attackDamage, float scale = 1.0f, EnemyBehavior behavior = EnemyBehavior::Melee);
    ~Enemy();
    void Update(float dt) override;
    void Draw() const override;
    void TakeDamage(float amount, Vector3 knockbackDir) override;
    void NotifyPlayerPosition(Vector3 playerPosition);

    // Igual que en Player: hitbox activa de este frame, o nullptr. CombatSystem
    // la testea contra el Player y cierra la ventana en el primer impacto.
    const Hitbox* GetActiveHitbox() const;
    void CloseAttackHitbox();

    // Asigna el shader a los materiales del cuerpo. La llama Application
    // tras construir el Enemy, una vez cargado el ShaderManager.
    void SetShader(Shader shader);

    // true una vez terminado el fade-out del cadáver (ver UpdateDead):
    // Application lo usa como criterio del erase-remove en m_level.enemies.
    bool IsPendingDestruction() const { return m_pendingDestruction; }

    // Patrón "consumir": true la primera vez que se llama tras generarse un
    // proyectil (Spitter) o completarse la detonación (Kamikaze), false el
    // resto de frames. Application los revisa una vez por enemigo y frame,
    // igual que ya hace con GetActiveHitbox().
    bool ConsumePendingProjectile(Projectile& outProjectile);
    bool ConsumeExplosionTrigger();

    float GetExplosionDamage() const { return m_attackDamage; }
    static constexpr float kExplodeRadius = 3.0f; // radio del AoE del Kamikaze; Application lo pasa a CombatSystem::ApplyAreaDamage

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

    StateMachine<EnemyState> m_fsm;
    Vector3 m_facingDirection = { 0.0f, 0.0f, 1.0f };
    std::vector<Vector3> m_patrolRoute;
    size_t m_currentPatrolIndex = 0;

    float m_speed;
    float m_attackDamage;
    float m_scale = 1.0f;
    EnemyBehavior m_behavior = EnemyBehavior::Melee;

    float m_visionRadius;
    Vector3 m_lastKnownPlayerPosition{};
    bool m_playerVisible = false;

    float m_attackCooldown = 0.0f;
    float m_hurtTimer = 0.0f;
    static constexpr float kAttackRange = 1.2f;
    static constexpr float kAttackInterval = 1.0f;
    static constexpr float kHurtDuration = 0.6f;

    // Hit-flash: destello breve independiente del tinte de estado (Hurt ya
    // tiñe de rojo durante toda su duración; esto es un pop de impacto de
    // 0.1s por encima de cualquier otro tinte, incluido Dead). Ver Draw().
    float m_damageFlashTimer = 0.0f;
    static constexpr float kDamageFlashDuration = 0.1f;

    // Spitter (AttackRanged): mantiene las distancias y dispara en vez de
    // cerrar hasta kAttackRange. Reutiliza m_attackCooldown/kAttackInterval
    // del Melee -- son mutuamente excluyentes, nunca coexisten en el mismo
    // Enemy, así que no hace falta duplicarlos con otro nombre.
    static constexpr float kRangedAttackRange = 8.0f;
    static constexpr float kRangedAttackInterval = 1.5f;
    static constexpr float kProjectileSpeed = 9.0f;
    bool m_pendingProjectile = false;
    Projectile m_queuedProjectile{};

    // Kamikaze (Explode): deja de moverse, parpadea cada vez más rápido y
    // detona a los kExplodeDuration segundos, matándose con su propio
    // TakeDamage (reutiliza toda la ruta normal de muerte/fade/limpieza).
    static constexpr float kKamikazeExplodeTriggerRange = 2.0f;
    static constexpr float kExplodeDuration = 1.5f;
    float m_explodeTimer = 0.0f;
    bool m_pendingExplosion = false;

    // Corpse cleanup: tiempo en el suelo tal cual, seguido de un fade-out
    // (ver Draw) antes de marcarse para destrucción. Sin esto, el Modo
    // Infinito acumularía cadáveres sin límite en m_level.enemies.
    float m_deathTimer = 0.0f;
    bool m_pendingDestruction = false;
    static constexpr float kCorpseGroundDuration = 3.0f;
    static constexpr float kCorpseFadeDuration = 1.5f;

    Hitbox m_activeHitbox{};
    bool m_hitboxWindowOpen = false;
    Model m_model;
    Sound m_hurtSound{};
    static constexpr float kHurtSoundVolume = 0.5f; // 0.0 (silencio) - 1.0 (volumen original del archivo)
};
