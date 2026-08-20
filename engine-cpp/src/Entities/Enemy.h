#pragma once
#include "Actor.h"
#include "../Core/FSM/StateMachine.h"
#include "../Core/CountdownTimer.h"
#include "../Combat/Hitbox.h"
#include "../Combat/Projectile.h"
#include <vector>

enum class EnemyState { Patrol, Chase, Attack, AttackRanged, Explode, Hurt, Dead };

// Perfil de combate del enemigo, resuelto por EnemyFactory a partir del
// campo "behavior" de enemy_variants.json (ver EnemyFactory.cpp). Determina
// a qué estado transiciona UpdateChase al alcanzar al jugador -- el resto de
// la FSM (Patrol/Hurt/Dead) es idéntico para todos.
//
//   Shielder: bloquea los golpes que le entran de frente (BlocksAttackFrom);
//             por la espalda o los flancos recibe daño normal, y el AoE de
//             barriles/Kamikaze lo atraviesa siempre.
//   Buffer:   no ataca -- mantiene las distancias y acelera a los zombis que
//             tenga dentro de su aura (ver Application::UpdateActiveMatch).
//   Trapper:  melee normal, pero va dejando charcos de lodo por donde pasa
//             (ver MudPuddle / ConsumePendingPuddle).
enum class EnemyBehavior { Melee, Kamikaze, Spitter, Shielder, Buffer, Trapper };

class Enemy : public Actor {
public:
    // scale multiplica tanto el modelo dibujado como el halfExtents de
    // colisión (ver constructor); por defecto 1.0f para no afectar a los
    // enemigos que ya construye LevelLoader desde el JSON de nivel. behavior
    // por defecto Melee, por la misma razón: un enemigo "Default" del JSON
    // de nivel no pasa por EnemyFactory y no debe comportarse como Spitter o
    // Kamikaze sin que nadie se lo haya pedido explícitamente. baseTint es el
    // código de color del arquetipo (mismo modelo 3D para todos, así que es
    // lo único que los distingue a distancia junto con la geometría de Draw).
    Enemy(Vector3 position, float maxHP, std::vector<Vector3> patrolRoute, float visionRadius,
          float speed, float attackDamage, float scale = 1.0f, EnemyBehavior behavior = EnemyBehavior::Melee,
          Color baseTint = WHITE);
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

    // Reaplica AudioSettings::GetSfxVolume() al Sound ya cargado -- ver
    // Player::RefreshSfxVolume, mismo motivo.
    void RefreshSfxVolume();

    // true una vez terminado el fade-out del cadáver (ver UpdateDead):
    // Application lo usa como criterio del erase-remove en m_level.enemies.
    bool IsPendingDestruction() const { return m_pendingDestruction; }

    // Patrón "consumir": true la primera vez que se llama tras generarse un
    // proyectil (Spitter) o completarse la detonación (Kamikaze), false el
    // resto de frames. Application los revisa una vez por enemigo y frame,
    // igual que ya hace con GetActiveHitbox().
    bool ConsumePendingProjectile(Projectile& outProjectile);
    bool ConsumeExplosionTrigger();

    // Mismo patrón: true la primera vez tras cumplirse el intervalo de goteo
    // del Trapper. Application crea el MudPuddle con la posición devuelta.
    bool ConsumePendingPuddle(Vector3& outPosition);

    float GetExplosionDamage() const { return m_attackDamage; }
    static constexpr float kExplodeRadius = 3.0f; // radio del AoE del Kamikaze; Application lo pasa a CombatSystem::ApplyAreaDamage

    EnemyBehavior GetBehavior() const { return m_behavior; }

    // true solo para un Shielder golpeado dentro de su cono frontal. La
    // consulta CombatSystem::ResolveMeleeAttack para descartar ESE golpe --
    // el AoE (ApplyAreaDamage) no la mira, una explosión rodea la placa.
    bool BlocksAttackFrom(Vector3 attackerPosition) const;

    // Multiplicador de velocidad del aura de un Buffer. Application lo
    // reescribe cada frame desde cero (1.0 = sin buff), así que el bonus
    // desaparece solo en cuanto el Buffer muere o el zombi sale del radio.
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

    // Compartido por UpdateChase, SpawnAttackHitbox y UpdateAttackRanged:
    // los tres calculaban el mismo vector normalizado hacia
    // m_lastKnownPlayerPosition por separado.
    Vector3 DirectionToLastKnownPlayer() const;

    // Velocidad efectiva de este frame (base × aura de Buffer). Todo
    // desplazamiento del Enemy pasa por aquí, igual que en el Player.
    float CurrentSpeed() const { return m_speed * m_speedMultiplier; }

    // Geometría adjunta que distingue al arquetipo por forma, no solo por
    // color: placa del Shielder, aro de aura del Buffer, depósito del
    // Trapper. Sin efecto para el resto de comportamientos.
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
    CountdownTimer m_damageFlashTimer;
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

    // Shielder: coseno del semiángulo del cono frontal protegido. 0.5 = ±60°
    // -- lo justo para que rodearlo sea una maniobra real, no un pixel-hunt.
    static constexpr float kShieldBlockCosine = 0.5f;

    // Buffer: no cierra distancia como un Melee, se queda a este radio del
    // jugador (desde donde su aura sigue cubriendo a la horda que empuja).
    static constexpr float kBufferKeepDistance = 5.0f;

    // Trapper: un charco cada este intervalo mientras siga vivo.
    static constexpr float kPuddleInterval = 2.5f;
    CountdownTimer m_puddleTimer;
    bool m_pendingPuddle = false;
    Vector3 m_queuedPuddlePosition{};

    // Corpse cleanup: fade-out continuo desde el instante de la muerte (ver
    // Draw) antes de marcarse para destrucción. Sin esto, el Modo Infinito
    // acumularía cadáveres sin límite en m_level.enemies.
    float m_deathTimer = 0.0f;
    bool m_pendingDestruction = false;
    static constexpr float kCorpseFadeDuration = 2.0f;

    Hitbox m_activeHitbox{};
    bool m_hitboxWindowOpen = false;
    Model m_model;
    Sound m_hurtSound{};
    Sound m_deathSound{};
};
