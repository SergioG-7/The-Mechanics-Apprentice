#pragma once
#include "raylib.h"
#include "Hitbox.h"
#include "Projectile.h"
#include "../Entities/MudPuddle.h"
#include <vector>
#include <memory>
#include <optional>

class Entity;
class Player;
class Enemy;
class ExplosiveBarrel;
class Hazard;
class ElectricTile;

// Resultado de un golpe conectado: el punto de impacto (juice) y un puntero
// no propietario al Enemy golpeado (para que Application pueda mirar si
// murió con este golpe, p.ej. para el drop de engranaje del Modo Infinito).
// Válido solo dentro del mismo frame -- CombatSystem no es dueño del Enemy.
struct MeleeHitResult {
    Vector3 impactPoint{};
    Enemy* hitEnemy = nullptr;

    // true si la placa de un Shielder paró el golpe: no hubo daño, pero SÍ
    // cuenta como impacto (cierra la ventana de hitbox y da su propio juice)
    // -- sin esto el swing seguiría probando cada frame contra un enemigo
    // que nunca puede recibirlo, sin ninguna señal de por qué.
    bool blocked = false;
};

class CombatSystem {
public:
    // Caja de golpe cuerpo a cuerpo centrada 'reach' unidades por delante de
    // origin, en la dirección 'direction' (normalizada) -- Player y Enemy
    // construían la misma BoundingBox campo a campo, solo cambiaban origen,
    // dirección, daño y fuerza de empuje.
    static Hitbox BuildMeleeHitbox(Vector3 origin, Vector3 direction, float damage, float knockbackForce,
                                    float reach = 1.0f, float halfExtent = 0.5f, float duration = 0.15f);

    // Cleave: golpea a TODOS los enemigos vivos que solapen la hitbox activa
    // en este frame, no solo al primero -- salvo que un Obstacle se
    // interponga en la línea de visión jugador-enemigo (CollisionMath::
    // SegmentIntersectsBoxXZ), en cuyo caso ese enemigo en concreto se
    // descarta: un golpe cuerpo a cuerpo no debe atravesar una pared aunque
    // el enemigo siga dentro del alcance de la hitbox. Devuelve un resultado
    // por enemigo golpeado (vacío si no había hitbox activa o no golpeó a
    // nadie) -- Application lo usa para disparar el juice (hit-stop, shake,
    // chispas por impacto) sin que CombatSystem necesite conocer nada de
    // cámara, partículas ni modos de juego.
    static std::vector<MeleeHitResult> ResolveMeleeAttack(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies,
                                                            const std::vector<std::unique_ptr<Entity>>& obstacles);

    // Simétrico a ResolveMeleeAttack pero contra barriles en vez de
    // enemigos. Solo se prueba si ResolveMeleeAttack no golpeó ya a ningún
    // enemigo este frame (la hitbox del Player se cierra en cuanto conecta
    // con al menos uno) -- así un golpe no puede dañar a la vez a un
    // enemigo y a un barril superpuestos.
    static std::optional<Vector3> ResolveMeleeAttackOnBarrels(Player& player, std::vector<std::unique_ptr<ExplosiveBarrel>>& barrels);

    // Simétrico a ResolveMeleeAttack: testea la hitbox activa de cada Enemy
    // vivo contra el AABB del Player. Un enemigo golpea como mucho una vez
    // por ventana de ataque (CloseAttackHitbox tras el impacto), igual que
    // el Player.
    static void ResolveEnemyAttacks(Player& player, std::vector<std::unique_ptr<Enemy>>& enemies);

    // Daño en área compartido por ExplosiveBarrel y el Kamikaze (Enemy):
    // ninguno de los dos conoce al Player, a los demás Enemy ni a otros
    // ExplosiveBarrel, así que la aplica quien sí los tiene a mano
    // (Application), a través de aquí. Incluye a los barriles para que un
    // Kamikaze (o un barril que ya explotó) pueda encadenar la explosión de
    // otro barril cercano.
    static void ApplyAreaDamage(Vector3 center, float radius, float damage, Player& player,
                                 std::vector<std::unique_ptr<Enemy>>& enemies,
                                 std::vector<std::unique_ptr<ExplosiveBarrel>>& barrels);

    // Mueve cada proyectil, comprueba impacto contra un ExplosiveBarrel,
    // contra obstacles (paredes/pilares) y contra el Player, y descarta
    // (erase-remove) los que impactan algo o expiran por tiempo de vida.
    // Orden de prioridad: obstáculo (bloqueo sólido, se prueba primero para
    // que proteja a lo que haya detrás) > barril (aplica TakeDamage, permite
    // que un Spitter encadene una explosión) > Player. Un impacto contra
    // cualquiera de los tres destruye el proyectil sin atravesarlo.
    static void UpdateProjectiles(float dt, std::vector<Projectile>& projectiles, Player& player,
                                   const std::vector<std::unique_ptr<Entity>>& obstacles,
                                   std::vector<std::unique_ptr<ExplosiveBarrel>>& barrels);

    // Un hazard no bloquea el paso (no vive en obstacles), así que a
    // diferencia del resto de combate esto no se dispara por colisión de
    // movimiento: se prueba una vez por frame contra el Player si sigue
    // dentro de su AABB cuando toca tick (ver Hazard::ConsumeTick). Empuja
    // con un knockback suave hacia afuera del centro -- mismo patrón que
    // ApplyAreaDamage. Deliberadamente NO daña a los Enemy: una trampa de
    // nivel es una amenaza para el jugador, no para la IA que ya la conoce.
    static void ApplyHazardDamage(std::vector<std::unique_ptr<Hazard>>& hazards, Player& player);

    // Envejece los charcos del Trapper y ralentiza al Player que pise uno
    // (ver MudPuddle). No hacen daño y no bloquean el paso -- son una
    // penalización de movilidad, no una trampa: por eso no van por
    // ApplyHazardDamage. Descarta (erase-remove) los ya caducados.
    static void UpdateMudPuddles(float dt, std::vector<MudPuddle>& puddles, Player& player);

    // Avanza cada baldosa, la arma si alguien la pisa y, en el frame de la
    // descarga, golpea UNA vez a todo lo que siga encima. A diferencia de
    // ApplyHazardDamage, incluye a los ENEMIGOS a propósito: poder cebar a un
    // zombi sobre una baldosa es la mitad de la mecánica.
    static void UpdateElectricTiles(float dt, std::vector<std::unique_ptr<ElectricTile>>& tiles,
                                     Player& player, std::vector<std::unique_ptr<Enemy>>& enemies);

    // Aura del Buffer (ver EnemyBehavior::Buffer): recalcula desde cero el
    // multiplicador de velocidad de TODOS los enemigos según qué Buffers
    // vivos los cubran. Vive aquí, con el resto de sistemas que necesitan ver
    // varias entidades a la vez (un Enemy no conoce a los demás), y no en
    // Application, que solo tiene que orquestar el orden de las llamadas.
    static void ApplyBufferAuras(std::vector<std::unique_ptr<Enemy>>& enemies);
};
