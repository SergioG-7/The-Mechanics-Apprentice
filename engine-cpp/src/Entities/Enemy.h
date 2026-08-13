#pragma once
#include "Actor.h"
#include "../Core/FSM/StateMachine.h"
#include "../Combat/Hitbox.h"
#include <vector>

enum class EnemyState { Patrol, Chase, Attack, Hurt, Dead };

class Enemy : public Actor {
public:
    Enemy(Vector3 position, float maxHP, std::vector<Vector3> patrolRoute, float visionRadius,
          float speed, float attackDamage);
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

private:
    void SetupStates();
    void UpdatePatrol(float dt);
    void UpdateChase(float dt);
    void EnterAttack();
    void UpdateAttack(float dt);
    void UpdateHurt(float dt);
    Hitbox SpawnAttackHitbox() const;

    StateMachine<EnemyState> m_fsm;
    Vector3 m_facingDirection = { 0.0f, 0.0f, 1.0f };
    std::vector<Vector3> m_patrolRoute;
    size_t m_currentPatrolIndex = 0;

    float m_speed;
    float m_attackDamage;

    float m_visionRadius;
    Vector3 m_lastKnownPlayerPosition{};
    bool m_playerVisible = false;

    float m_attackCooldown = 0.0f;
    float m_hurtTimer = 0.0f;
    static constexpr float kAttackRange = 1.2f;
    static constexpr float kAttackInterval = 1.5f;
    static constexpr float kHurtDuration = 0.6f;

    Hitbox m_activeHitbox{};
    bool m_hitboxWindowOpen = false;
    Model m_model;
    Sound m_hurtSound{};
    static constexpr float kHurtSoundVolume = 0.5f; // 0.0 (silencio) - 1.0 (volumen original del archivo)
};
