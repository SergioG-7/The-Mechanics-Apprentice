# The Mechanic's Apprentice

## Arquitectura de un motor propio en C++ y su herramienta de autoría en C#

> Documento de arquitectura técnica. Explica **por qué el código es como es**, no qué hace el juego.
> Continúa —y da por leído— el documento de arquitectura de `tactical-soccer` (Unity / C#).

| | |
|---|---|
| **Proyecto** | `The-Mechanic-s-Apprentice` |
| **Motor** | Propio, sobre raylib 6.0 (`FetchContent`) + nlohmann/json 3.12.0 · CMake ≥ 3.16 · MSVC `/W4`, 0 warnings |
| **Runtime del juego** | C++17 — 6.264 líneas en 72 ficheros (`engine-cpp/src`) + 51 de GLSL |
| **Herramienta de autoría** | C# `net9.0-windows` / WinForms sin diseñador — 2.518 líneas en 27 ficheros |
| **Datos** | 16 niveles + 1 catálogo de arquetipos en JSON · 3 idiomas (ES/EN/JA) en ambos binarios |
| **Fecha** | 21 de agosto de 2026 |

---

## Cómo leer este documento

### Qué se da por sabido

El lector es ingeniero informático. Además, **se da por leído el documento técnico del proyecto anterior** (`tactical-soccer/DOCUMENTACION-TECNICA.pdf`, *«Anatomía de un minijuego de fútbol táctico en Unity»*), y por tanto este texto **no vuelve a explicar**:

- el modelo de ejecución de un motor de videojuegos (bucle de frames y callbacks en lugar de `main()`, composición `GameObject` + `Component`, ciclo de vida);
- `deltaTime`, escalas de tiempo y por qué un juego integra por frame;
- corrutinas y concurrencia cooperativa;
- máquinas de estados finitos aplicadas a entidades (el `StateMachine.h` de este proyecto es la misma idea ya documentada allí);
- patrones de creación y acceso global (Singleton, factorías, buses de eventos);
- la serialización de datos de juego como concepto, ni la localización como problema.

Lo que sí se explica es **todo lo que cambia al salir de Unity**: quién libera la memoria cuando no hay recolector de basura, quién es dueño de qué, qué garantiza el lenguaje y qué no, qué aparece cuando la herramienta de edición deja de ser un panel del propio motor y pasa a ser **otro proceso, en otro lenguaje, con otro runtime**, y cómo se verifica contenido que ya no lo produce una persona colocando objetos a mano.

Las cuatro partes son independientes entre sí y pueden leerse en cualquier orden, aunque §2 (el contrato de datos) es la que da sentido a §3 y §4.

### Convención tipográfica

Los identificadores `en esta tipografía` son reales: se pueden buscar con `grep` en el repositorio. Los bloques de código son **extractos textuales** de los ficheros indicados al pie, con comentarios recortados solo por espacio. Cuando un comentario aparece completo es porque **la justificación de la decisión vive en el propio código**, que es una convención deliberada del proyecto.

### Índice

1. [De Unity (C#) a motor propio (C++): gestión de memoria](#1-de-unity-c-a-motor-propio-c-gestión-de-memoria)
2. [Arquitectura data-driven: desacoplamiento total](#2-arquitectura-data-driven-desacoplamiento-total)
3. [Herramientas propias: interoperación con C# WinForms](#3-herramientas-propias-interoperación-con-c-winforms)
4. [Algoritmia aplicada: validación de niveles por flood-fill](#4-algoritmia-aplicada-validación-de-niveles-por-flood-fill)
5. [Apéndices](#5-apéndices)

### El sistema en una página

Dos artefactos ejecutables independientes, sin ninguna dependencia binaria entre ellos:

```
                   ┌───────────────────────────────┐
                   │   assets/data/*.json          │   <- el CONTRATO
                   │   level_1..15, endless,       │      (esquema estable)
                   │   enemy_variants              │
                   └───────────────────────────────┘
                      ^ escribe              │ lee
                      │                      v
    ┌─────────────────────────┐   ┌──────────────────────────────┐
    │  LevelEditor.exe (C#)   │   │  Level5Portfolio.exe (C++)   │
    │  WinForms, net9.0       │   │  raylib 6.0, C++17           │
    │  ─────────────────────  │   │  ──────────────────────────  │
    │  EditorScene (modelo)   │   │  LevelLoader -> LevelData    │
    │  CanvasRenderer (vista) │   │  Application (bucle, estado) │
    │  PropertyPanelBuilder   │   │  Entity/Actor · CombatSystem │
    └─────────────────────────┘   └──────────────────────────────┘
              ^                                  │
              └──────────────────────────────────┘
                 std::system("start …") + argv[0] = idioma
                 (único acoplamiento en tiempo de ejecución)
```

Los dos leen el **mismo** `enemy_variants.json`: el editor lo enlaza desde el árbol del motor (`<None Include="..\engine-cpp\assets\data\enemy_variants.json" Link="…">`), no lo copia. No puede haber dos verdades sobre cuánta vida tiene un `Tank`.

---

## 1. De Unity (C#) a motor propio (C++): gestión de memoria

### 1.1 El cambio de contrato

En el proyecto anterior, la vida de un objeto era un problema del runtime: `Destroy(gameObject)` marcaba, el recolector barría, y una referencia caducada se manifestaba como una `MissingReferenceException` legible. Aquí no hay nada de eso. Al desaparecer el recolector aparecen tres preguntas que **el diseño tiene que contestar explícitamente para cada objeto**:

1. **¿Quién es el dueño?** — quién decide cuándo deja de existir.
2. **¿Cuándo se libera?** — en qué punto exacto del frame, y en qué orden respecto a otros recursos.
3. **¿Quién más lo está mirando?** — qué punteros no propietarios existen, y cómo se garantiza que ninguno sobreviva al objeto.

La respuesta del proyecto a las dos primeras es RAII. La tercera es la que da problemas de verdad, y tiene sección propia (§1.6 y §1.7).

### 1.2 RAII: el destructor como única vía de liberación

**RAII** (*Resource Acquisition Is Initialization*) consiste en atar la vida de un recurso a la vida de un objeto automático: se adquiere en el constructor y se libera en el destructor. Como el lenguaje **garantiza** la ejecución del destructor al salir del ámbito —incluida la salida por excepción—, no existe ningún camino de código en el que el recurso quede sin liberar. No hay `finally`, no hay `using`, no hay disciplina que recordar: la garantía es del compilador.

El ejemplo canónico del proyecto es el shader. raylib expone un par `LoadShader` / `UnloadShader` sobre un `struct Shader` que es, en esencia, un identificador de GPU: exactamente el tipo de recurso que se filtra si alguien olvida una llamada.

```cpp
// Wrapper RAII sobre un Shader de raylib. Carga un par vertex/fragment GLSL
// y garantiza UnloadShader() al destruirse.
class ShaderManager {
public:
    ShaderManager(const std::string& vsPath, const std::string& fsPath);
    ~ShaderManager();

    ShaderManager(const ShaderManager&) = delete;
    ShaderManager& operator=(const ShaderManager&) = delete;

    Shader Get() const { return m_shader; }

private:
    Shader m_shader{};
};
```
*— `engine-cpp/src/Renderer/ShaderManager.h`*

```cpp
ShaderManager::ShaderManager(const std::string& vsPath, const std::string& fsPath) {
    m_shader = LoadShader(vsPath.c_str(), fsPath.c_str());
    if (m_shader.id == 0) {
        TraceLog(LOG_WARNING, "ShaderManager: fallo al compilar (%s, %s)",
                  vsPath.c_str(), fsPath.c_str());
    }
}

ShaderManager::~ShaderManager() {
    if (m_shader.id != 0) {      // un shader que no llegó a compilar no se descarga
        UnloadShader(m_shader);
    }
}
```
*— `engine-cpp/src/Renderer/ShaderManager.cpp`*

Las dos líneas importantes son las **borradas**. `Shader` es un `struct` trivialmente copiable: sin `= delete`, copiar un `ShaderManager` produciría dos objetos con el mismo `id` de GPU y **doble liberación** al destruirse ambos —un fallo que no se manifiesta en la línea que copia, sino mucho después—. Declarar el destructor, además, suprime la generación implícita de constructor y asignación por movimiento, así que el tipo pasa a ser **inmóvil**: la única forma de tenerlo dentro de otro objeto y decidir *cuándo* nace es a través de un puntero inteligente, que es justo lo que hace `Application`:

```cpp
std::unique_ptr<MusicController> m_music;
std::unique_ptr<ShaderManager> m_toonShader;
```
*— `engine-cpp/src/Core/Application.h`*

El motivo documentado en el constructor es de **orden de inicialización**, no de estilo:

```cpp
// Construido en el cuerpo, no en la lista de inicialización: necesita
// InitAudioDevice() ya llamado (arriba), igual que m_toonShader necesita
// la ventana/contexto GL ya creados por InitWindow().
m_music = std::make_unique<MusicController>(FindMusicFile(), m_saveManager.Data().bgmVolume);
```
*— `engine-cpp/src/Core/Application.cpp`*

Los miembros por valor se construyen **antes** de que empiece el cuerpo del constructor; un miembro que necesita un contexto global ya inicializado (GL, audio) no puede serlo. `MusicController` sigue el mismo patrón que `ShaderManager` para el `Music` de fondo, con copia borrada por la misma razón.

`MenuScreen` y `LocalizationManager` sí son miembros por valor, y el proyecto resuelve su dependencia del contexto partiendo la construcción en dos fases explícitas —constructor barato, más un `LoadSfx()` / `LoadFonts()` que `Application` llama cuando el contexto ya existe—. Es la alternativa a envolverlo todo en punteros: **inicialización en dos fases, con la fase pesada visible en la línea de llamada**, en vez de indirección adicional.

### 1.3 Propiedad jerárquica: `unique_ptr` como declaración de intenciones

El nivel entero es un agregado de punteros únicos:

```cpp
struct LevelData {
    std::unique_ptr<Player> player;
    std::vector<std::unique_ptr<Enemy>> enemies;

    // Geometría estática que SÍ bloquea el movimiento: Obstacle (caja) y
    // Cylinder (pilar) mezclados en la misma lista a propósito -- ambos son
    // Entity con un AABB, así que Entity::TryMove los trata igual sin
    // distinguir tipos.
    std::vector<std::unique_ptr<Entity>> obstacles;

    std::vector<std::unique_ptr<Gear>> gears;
    std::unique_ptr<Door> door;          // nullptr si el nivel no define puerta
    std::vector<SpawnerData> spawners;   // datos planos, NO entidades
    std::vector<std::unique_ptr<HealthKit>> healthKits;
    std::vector<std::unique_ptr<ExplosiveBarrel>> barrels;
    std::vector<std::unique_ptr<PowerUp>> powerUps;
    std::vector<std::unique_ptr<Hazard>> hazards;
    std::vector<std::unique_ptr<ElectricTile>> electricTiles;
};
```
*— `engine-cpp/src/IO/LevelLoader.h`*

Cuatro decisiones merecen comentario, porque quien venga de C# tenderá a leerlas como ruido sintáctico:

**a) `vector<unique_ptr<Entity>>` y no `vector<Entity>`.** No es preferencia: es corrección. Un `vector<Entity>` almacena objetos del tamaño exacto de `Entity`; asignarle un `Cylinder` lo truncaría (*object slicing*), perdiendo sus campos y su tabla virtual. En C++ el polimorfismo solo funciona a través de puntero o referencia. El precio es una indirección por acceso y peor localidad de caché —irrelevante con decenas de entidades, decisivo con cientos de miles—.

**b) `virtual ~Entity() = default;`.** Sin ella, `delete` sobre un `Entity*` que en realidad apunta a un `Cylinder` es **comportamiento indefinido**: se ejecutaría el destructor de la base y los miembros de la derivada no se liberarían. Es una línea que no hace nada visible y sin la cual todo el esquema del punto anterior es inseguro.

**c) `SpawnerData` es un `struct` por valor.** Los generadores de oleadas no son entidades: no ocupan espacio, no colisionan, no tienen vida. Lo que el JSON produce es un registro plano que `Application::LoadLevel` convierte después en `Spawner` reales. Que el tipo del contenedor lo delate es intencionado.

**d) Descarga por asignación.** Vaciar el nivel entero es una línea:

```cpp
m_level = LevelData{};   // move-assignment implícita: destruye todo lo anterior
```

`LevelData` no declara destructor propio, así que el compilador genera la asignación por movimiento; asignar un temporal vacío destruye en cadena todos los `unique_ptr` anteriores. Y como **cada destructor de entidad libera sus propios recursos de GPU y audio**, una sola asignación libera modelos, texturas y sonidos de todo el nivel.

### 1.4 La trampa del orden: destructores de miembros contra contextos globales

Aquí está el matiz de RAII que no sale en los manuales y que este proyecto documenta en el propio código. raylib mantiene un contexto global de ventana/GL y otro de audio. Todos los recursos (modelos, texturas, sonidos, fuentes, shaders) **pertenecen a esos contextos**: liberarlos después de cerrarlos no es tardío, es incorrecto.

Y los miembros de una clase se destruyen **después** de que termine el cuerpo de su destructor. Es decir: un destructor que solo contuviera `CloseAudioDevice(); CloseWindow();` estaría cerrando los contextos **antes** de destruir los miembros que dependen de ellos.

```cpp
Application::~Application() {
    // Los recursos de GPU/audio (modelos, texturas y sonidos de m_level,
    // el shader, la música, la fuente) tienen que liberarse ANTES de cerrar
    // el contexto que los sostiene. Dejarlo a la destrucción automática de
    // miembros habría sido incorrecto: esa destrucción ocurre DESPUÉS de
    // que termine el cuerpo de este destructor, es decir, después de
    // CloseAudioDevice()/CloseWindow() si esas dos llamadas fueran lo único
    // aquí -- liberando GPU/audio contra un contexto ya cerrado. Se liberan
    // aquí a mano, en el orden correcto, antes de cerrar nada.
    m_level = LevelData{};         // 1. entidades: modelos, texturas, sonidos
    m_toonShader.reset();          // 2. shader
    m_music.reset();               // 3. stream de música
    m_localization.UnloadFonts();  // 4. fuentes (atlas de textura en GPU)

    CloseAudioDevice();            // 5. y solo ahora, los contextos
    CloseWindow();
}
```
*— `engine-cpp/src/Core/Application.cpp`*

La lección generalizable: **RAII resuelve el «si», no siempre el «cuándo»**. Cuando existe un recurso global que es *ámbito* de los demás, hay que ordenar la liberación a mano —o modelar ese contexto como el primer miembro de la clase, que se destruiría el último—. El proyecto escogió lo explícito porque los contextos de raylib son funciones libres, no objetos.

### 1.5 Cuando la biblioteca cambia el contrato: raylib 6.0 y las texturas

`UnloadModel()` dejó de liberar las texturas de los materiales en raylib 6.0 —cambio deliberado, para no invalidar texturas compartidas entre modelos—. Un destructor que llame solo a `UnloadModel` compila, no avisa y **filtra un atlas de varios megabytes por modelo**.

```cpp
void UnloadOwnTextures(const Model& model) {
    for (int i = 0; i < model.materialCount; i++) {
        Texture2D albedo = model.materials[i].maps[MATERIAL_MAP_ALBEDO].texture;
        // La textura por defecto de raylib es global y compartida: liberarla
        // corrompería el estado del resto del programa.
        if (albedo.id != rlGetTextureIdDefault()) UnloadTexture(albedo);
    }
}

void UnloadModelAndTextures(Model& model) {
    UnloadOwnTextures(model);
    UnloadModel(model);
}
```
*— `engine-cpp/src/Renderer/ModelUtils.cpp`*

```cpp
Player::~Player() {
    ModelUtils::UnloadModelAndTextures(m_model);
    ModelUtils::UnloadModelAndTextures(m_weaponModel);
    if (m_attackSound.frameCount > 0) UnloadSound(m_attackSound);
    if (m_hurtSound.frameCount > 0)   UnloadSound(m_hurtSound);
    if (m_dashSound.frameCount > 0)   UnloadSound(m_dashSound);
}
```
*— `engine-cpp/src/Entities/Player.cpp`*

Dos observaciones de ingeniería:

- El guardián `frameCount > 0` distingue «sonido cargado» de «sonido que falló al cargar». Un tipo C sin estado de validez obliga a inventarse el centinela; es exactamente el trabajo que un tipo RAII propio (como `ShaderManager`) hace una vez y para siempre. Aquí no se hizo porque son cinco campos en dos clases, y el criterio del proyecto es no introducir abstracción por debajo de su umbral de rentabilidad.
- El comportamiento **se verificó leyendo `rmodels.c`**, no asumiéndolo. Con una biblioteca C sin RAII, el contrato de liberación es parte de la documentación y **cambia entre versiones**.

### 1.6 Borrado durante la iteración: `erase-remove` y una cascada

En C# se marca un objeto y el recolector lo retira algún día. En C++ hay que sacarlo de su contenedor, y hacerlo **mientras se recorre** invalida iteradores. La solución idiomática es el *erase-remove*: `std::remove_if` **no borra**; reordena el rango dejando los supervivientes al principio y devuelve el nuevo final lógico, que es lo que `erase` recorta en una sola operación. Coste *O(n)* con un único desplazamiento, frente a *O(n²)* de ir borrando de uno en uno.

```cpp
m_level.barrels.erase(
    std::remove_if(m_level.barrels.begin(), m_level.barrels.end(),
                    [](const std::unique_ptr<ExplosiveBarrel>& b) { return b->HasExploded(); }),
    m_level.barrels.end());
```
*— `engine-cpp/src/Core/Application.cpp`*

Y aquí es donde el proyecto se topó con un fallo real que ilustra por qué **un flag de estado no sirve como cola de trabajo**. El bucle que aplicaba el daño en área usaba `HasExploded()` como criterio de «pendiente de resolver». Pero la explosión de un barril puede detonar a otro situado **antes** en el vector, que el bucle ya había dejado atrás: quedaba marcado, el `erase-remove` del final del frame se lo llevaba, y su área **nunca llegaba a aplicarse**. Una explosión que se veía y no hacía daño.

La corrección separa «ha ocurrido» de «ya se ha procesado», con un consumo de un solo uso:

```cpp
    // true EXACTAMENTE una vez por barril, en la primera llamada tras
    // explotar -- patrón "consumir".
    //
    // Existe porque HasExploded() por sí solo no distinguía "ha explotado" de
    // "ya se le ha aplicado el área".
    bool ConsumeExplosion();
private:
    bool m_exploded = false;
    bool m_explosionResolved = false;
```
*— `engine-cpp/src/Entities/ExplosiveBarrel.h`*

```cpp
void Application::ResolveBarrelExplosions() {
    // Se repite hasta que ningún barril quede por resolver, en vez de una
    // sola pasada: la explosión de un barril puede detonar a otro que está
    // ANTES en el vector y que este bucle ya ha dejado atrás.
    //
    // ConsumeExplosion() es de un solo uso por barril, así que el bucle
    // TERMINA siempre: cada vuelta resuelve al menos uno y ninguno se puede
    // resolver dos veces.
    bool resolvedAny = true;
    while (resolvedAny) {
        resolvedAny = false;

        // Índice, no iterador/referencia: ApplyAreaDamage no toca el tamaño
        // del vector, pero recorrerlo por índice deja explícito que aquí no
        // se puede invalidar nada aunque en el futuro sí lo tocara.
        for (size_t i = 0; i < m_level.barrels.size(); i++) {
            if (!m_level.barrels[i]->ConsumeExplosion()) continue;

            Vector3 center = m_level.barrels[i]->GetPosition();
            CombatSystem::ApplyAreaDamage(center, ExplosiveBarrel::kExplosionRadius,
                                           ExplosiveBarrel::kExplosionDamage,
                                           *m_level.player, m_level.enemies, m_level.barrels);
            m_particles.Emit(center, GetRandomValue(15, 25));
            resolvedAny = true;
        }
    }
}
```
*— `engine-cpp/src/Core/Application.cpp`*

Es un cálculo de **punto fijo**: se itera hasta que una pasada completa no produce cambios. La terminación no se argumenta con un contador máximo sino con un invariante —el flag es monótono y de un solo uso—, que es la forma robusta de garantizarla. La elección de índice sobre iterador es defensa frente a la invalidación futura: si algún día `ApplyAreaDamage` insertara o borrara en `m_level.barrels`, un iterador guardado sería un puntero colgante silencioso.

El patrón es reutilizable tal cual para cualquier cascada dentro de un mismo contenedor: propagación de daño, encadenados, difusión.

### 1.7 Punteros observadores: el invariante anti-*dangling*

Los `Spawner` necesitan saber cuántos de **sus** enemigos siguen vivos para respetar su cupo, pero no son sus dueños: la propiedad vive en `LevelData::enemies`. El tipo lo declara:

```cpp
    // Enemigos vivos generados por ESTE spawner, por puntero no propietario
    // (la propiedad real vive en activeEnemies) -- solo para poder contar
    // cuántos de los suyos siguen vivos y respetar m_maxEnemies.
    std::vector<Enemy*> m_spawnedEnemies;
```
*— `engine-cpp/src/Entities/Spawner.h`*

Un `Enemy*` desnudo conviviendo con `unique_ptr` es la fuente clásica de punteros colgantes. El diseño ya era correcto —`Spawner::Update` purgaba por `IsAlive()` cada frame, antes del borrado—, pero **su corrección dependía del orden de dos llamadas dentro de un bucle de noventa líneas**. Cualquiera que reordenara ese bucle rompería el invariante sin que nada lo señalase.

La solución no fue cambiar la lógica, sino **mover el invariante junto al punto de peligro**:

```cpp
    // Los Spawner guardan Enemy* NO propietarios de los que han generado.
    // Se les hace soltar los que van a desaparecer AQUÍ, pegado al
    // erase-remove, en vez de confiar en que su propio Update (que corre
    // antes en este mismo frame) ya los haya purgado por IsAlive(): así el
    // invariante "ningún puntero sobrevive a su Enemy" vive junto al borrado
    // y no se rompe si algún día se reordena este bucle.
    for (Spawner& spawner : m_spawners) spawner.ForgetDestroyedEnemies();

    m_level.enemies.erase(
        std::remove_if(m_level.enemies.begin(), m_level.enemies.end(),
                        [](const std::unique_ptr<Enemy>& e) { return e->IsPendingDestruction(); }),
        m_level.enemies.end());
```
*— `engine-cpp/src/Core/Application.cpp`*

Es defensa en profundidad y cuesta una pasada *O(n)* por frame sobre una lista de unidades. La alternativa «académica» —`weak_ptr`, que detecta la caducidad en tiempo de ejecución— obligaría a convertir toda la propiedad a `shared_ptr`, es decir, a pagar recuento atómico de referencias en **todas** las entidades para resolver un problema que solo tienen cuatro punteros observadores. La regla que sigue el proyecto: *si el dueño es único y conocido, `unique_ptr` más purga adyacente al borrado; `shared_ptr` solo cuando la propiedad sea genuinamente compartida* —que aquí no ocurre en ningún sitio—.

### 1.8 Resumen: quién posee qué

| Recurso | Dueño | Liberación | Observadores |
|---|---|---|---|
| Ventana / contexto GL / audio | `Application` | Explícita, al final de `~Application` | Todo el motor |
| Shader toon | `unique_ptr<ShaderManager>` | RAII (`~ShaderManager`) | `Player`, `Enemy`, `Spawner` (copia del handle) |
| Música de fondo | `unique_ptr<MusicController>` | RAII | — |
| Modelos, texturas y sonidos de entidad | Cada entidad | Destructor propio (+ `ModelUtils`) | — |
| Entidades del nivel | `LevelData` (`unique_ptr`) | `m_level = LevelData{}` o `erase-remove` | `Spawner::m_spawnedEnemies` (`Enemy*`) |
| Proyectiles, charcos de lodo | `vector<T>` por valor en `Application` | `erase-remove` por caducidad | — |
| Obstáculos (para colisión) | `LevelData::obstacles` | Con el nivel | `Actor` (puntero **al vector**, no a sus elementos) |

La última fila es una técnica que conviene señalar: `Actor` guarda un puntero **al contenedor**, no a sus elementos, y el contenedor vive exactamente lo mismo que el nivel. Es la forma más barata de compartir un conjunto de solo lectura sin duplicarlo ni gestionar su vida.
---

## 2. Arquitectura data-driven: desacoplamiento total

### 2.1 Qué significa aquí «data-driven»

No es «los números están en un fichero». Es una propiedad más fuerte y comprobable:

> **Ningún valor de contenido aparece en el código fuente del motor. Añadir un nivel, un arquetipo de enemigo o una mecánica de composición no requiere recompilar.**

El binario contiene **mecánicas**; los datos contienen **contenido**. La frontera es explícita y solo hay dos ficheros de código autorizados a cruzarla: `IO/LevelLoader.cpp` (topología del nivel) y `Entities/EnemyFactory.cpp` (estadísticas de arquetipos).

| Dato | Vive en | Lo consume |
|---|---|---|
| Topología del nivel (geometría, entidades, objetivos) | `assets/data/level_<N>.json`, `endless.json` | `LevelLoader::LoadFromFile` |
| Estadísticas y comportamiento por arquetipo | `assets/data/enemy_variants.json` | `EnemyFactory::LoadVariants` |
| Texto de interfaz | `assets/lang/{es,en,jp}.json` | `LocalizationManager` |
| Progreso y ajustes del jugador | `save_data.json` (junto al ejecutable) | `SaveManager` |
| Constantes de *mecánica* (cooldowns, radios, umbrales) | `static constexpr` en la clase que las usa | El compilador |

La última fila es la contrapartida honesta del diseño: la duración del *dash* o el radio de una explosión **no** son data-driven, y es deliberado. Exponerlos como datos obligaría a validar rangos en tiempo de ejecución y permitiría estados inconsistentes que ninguna mecánica soporta. La regla operativa: *si cambiarlo puede romper una invariante del código, es una constante; si solo cambia el contenido, es un dato*.

Un único parámetro se ha movido de un lado al otro de esa frontera, y explica bien el criterio: `turnRateDegPerSec` empezó siendo una constante y pasó al JSON cuando hizo falta que **un solo arquetipo** (el `Shielder`) girase despacio. Al ser dato, su valor por defecto —0, giro instantáneo— mantiene el comportamiento de los seis arquetipos restantes sin tocarlos.

### 2.2 La entrada es hostil: parseo tolerante por defecto

`nlohmann/json` ofrece dos accesos con semánticas opuestas, y de cuál se use en cada punto depende la robustez del sistema entero:

| Acceso | Falta la clave | Uso en este proyecto |
|---|---|---|
| `node.at("k")` | **lanza** `json::exception` | Solo para lo estructuralmente obligatorio |
| `node.value("k", def)` | devuelve `def` | Todo lo demás |

El proyecto usa `.at()` en **un único sitio**:

```cpp
// El jugador es el ÚNICO campo obligatorio: sin él no hay partida que
// construir, así que su ausencia sí debe abortar la carga (Application
// lo detecta por level.player == nullptr y vuelve al menú). Sus stats,
// en cambio, tienen defaults como todo lo demás.
const json& playerNode = root.at("player");
level.player = std::make_unique<Player>(
    ParseVector3Field(playerNode, "spawn", Vector3{ 0.0f, 0.0f, 0.0f }),
    playerNode.value("maxHP", 100.0f),
    playerNode.value("speed", 4.5f),
    playerNode.value("attackDamage", 45.0f));
```
*— `engine-cpp/src/IO/LevelLoader.cpp`*

Los vectores tridimensionales —la estructura que más se repite en el esquema— pasan todos por un único punto de entrada con doble comprobación de tipo y de presencia:

```cpp
Vector3 ParseVector3(const json& node) {
    // .value(), no .at(): un vector al que le falte una componente (JSON
    // editado a mano) cae a 0 en ese eje en vez de lanzar. Serializado por el
    // editor C# nunca pasa -- Vector3Data siempre escribe las tres.
    return Vector3{ node.value("x", 0.0f), node.value("y", 0.0f), node.value("z", 0.0f) };
}

// Vector3 de una clave OPCIONAL del nodo. Si la clave falta o no es un
// objeto, devuelve fallback en vez de lanzar. Es el punto único por el que
// pasan todas las posiciones/tamaños del nivel, así que ninguna entrada mal
// formada puede tirar abajo la carga entera.
Vector3 ParseVector3Field(const json& node, const char* key, Vector3 fallback) {
    if (!node.contains(key) || !node.at(key).is_object()) return fallback;
    return ParseVector3(node.at(key));
}
```
*— `engine-cpp/src/IO/LevelLoader.cpp`*

Nótese `!node.at(key).is_object()`: `contains()` por sí solo devuelve `true` para `"position": null`, y el parseo de abajo trabajaría sobre un nulo. No es paranoia teórica —§2.5 muestra el caso real que lo provocó—.

### 2.3 El fallo de granularidad: `try`/`catch` por entrada

Este es el punto arquitectónicamente más interesante de la sección, porque el código *ya tenía* manejo de errores y aun así estaba mal.

La versión original envolvía toda la carga en un único `try`/`catch`. Consecuencia: **un solo campo mal escrito, en una sola entrada de una sola lista, vaciaba el nivel entero**. `Application` interpretaba el `LevelData` vacío como «nivel no disponible», rebotaba al menú principal, y el log no decía qué entrada de qué lista era la culpable. El manejo de errores existía; lo que estaba mal era su **granularidad**: el ámbito del `catch` era mucho mayor que el ámbito del daño.

La corrección introduce una plantilla que recorre cualquier array opcional del nivel aislando cada elemento:

```cpp
// Recorre un array opcional del nivel aplicando parse a cada elemento, con
// try/catch POR ELEMENTO. Antes, un solo campo mal escrito en una entrada
// hacía saltar el catch global de LoadFromFile y el nivel ENTERO volvía
// vacío -- Application lo interpretaba como "nivel no disponible" y rebotaba
// al menú, sin pista de qué entrada de qué lista era la culpable. Ahora se
// salta esa entrada con un aviso que la identifica y el resto del nivel carga.
template <typename ParseFn>
void ParseArray(const json& root, const char* key, ParseFn parse) {
    if (!root.contains(key) || !root.at(key).is_array()) return;   // lista ausente = lista vacía

    const json& array = root.at(key);
    for (size_t i = 0; i < array.size(); i++) {
        try {
            parse(array[i]);
        } catch (const json::exception& e) {
            TraceLog(LOG_WARNING, "LevelLoader: '%s'[%d] mal formado, se omite (%s)",
                     key, static_cast<int>(i), e.what());
        }
    }
}
```
*— `engine-cpp/src/IO/LevelLoader.cpp`*

Tres propiedades que hacen que esto funcione:

1. **Aislamiento del fallo.** El radio de daño de una entrada corrupta es esa entrada. El nivel carga con un objeto menos, que es degradación aceptable; volver al menú, no.
2. **Diagnóstico accionable.** El mensaje identifica *lista* e *índice*: `"enemies[3] mal formado, se omite"` es directamente localizable en el fichero. Un `catch` global solo podía decir «el nivel está mal».
3. **Lista ausente ≡ lista vacía.** El primer `return` implica que un nivel de 2025 sin `"powerUps"`, `"electricTiles"` ni `"weights"` sigue cargando hoy sin migración. El esquema es **aditivo**: campos nuevos son siempre opcionales con valor por defecto que reproduce el comportamiento anterior.

El uso queda declarativo —cada lista es una línea y una lambda—:

```cpp
ParseArray(root, "gears", [&level](const json& n) {
    level.gears.push_back(std::make_unique<Gear>(
        ParseVector3Field(n, "position", Vector3{ 0.0f, 0.0f, 0.0f })));
});

ParseArray(root, "electricTiles", [&level](const json& n) {
    level.electricTiles.push_back(std::make_unique<ElectricTile>(
        ParseVector3Field(n, "position", Vector3{ 0.0f, 0.0f, 0.0f }),
        ParseVector3Field(n, "size",     Vector3{ 2.0f, 0.1f, 2.0f }),
        n.value("damage", 20.0f),
        n.value("cycleInterval", 0.0f)));
});
```
*— `engine-cpp/src/IO/LevelLoader.cpp`*

El `catch` global se conserva por debajo, pero su papel cambia: ya no atrapa errores de contenido, sino de **sintaxis** —un fichero que ni siquiera es JSON válido—, que sí es un fallo del documento entero y merece abortar.

### 2.4 Degradación en cascada, nunca fallo duro

Todo el pipeline de datos sigue la misma política, aplicada en tres niveles:

| Nivel del fallo | Ejemplo | Reacción |
|---|---|---|
| Campo | Falta `"maxHP"` en un enemigo | Valor por defecto, sin aviso |
| Campo con semántica | `"behavior": "spitr"` (typo) | Cae a `melee` **con** `TraceLog(LOG_WARNING)` |
| Entrada | `"position"` es una cadena | Se omite esa entidad, aviso con lista e índice |
| Referencia cruzada | `"type": "Ninja"` no existe en el catálogo | Se construye con sus propios stats del nivel, aviso |
| Documento | JSON sintácticamente inválido | Nivel vacío, aviso; `Application` vuelve al menú |
| Estructura | Falta `"player"` | Nivel vacío: no hay partida posible |

El nivel «referencia cruzada» es el que mejor muestra la filosofía. Un arquetipo desconocido **no** aborta nada:

```cpp
enemy = EnemyFactory::CreateEnemy(type, ParseVector3Field(n, "spawn", …), patrolRoute);
if (!enemy) {
    TraceLog(LOG_WARNING,
             "LevelLoader: enemigo con type '%s' no encontrado en enemy_variants.json, "
             "usando sus propios stats del JSON", type.c_str());
    enemy = BuildEnemyFromOwnStats(n, std::move(patrolRoute));
}
```
*— `engine-cpp/src/IO/LevelLoader.cpp`*

Obsérvese el detalle de propiedad en esas dos llamadas: a `CreateEnemy` se le pasa `patrolRoute` **por copia** y al *fallback* por `std::move`. Es intencionado y está comentado en el fuente: si el primer camino consumiera la ruta por movimiento, el segundo trabajaría sobre un vector vaciado. Es exactamente el tipo de error que en C# no existe y que en C++ hay que razonar en cada bifurcación.

### 2.5 El catálogo de arquetipos: una sola fuente de verdad

`enemy_variants.json` define los siete arquetipos con sus estadísticas y su comportamiento:

```json
{
  "variants": {
    "Tank":    { "maxHP": 200, "speed": 1.2, "scale": 1.5, "attackDamage": 25,
                 "visionRadius": 5,  "tint": [150, 150, 165] },
    "Runner":  { "maxHP": 25,  "speed": 6.5, "scale": 0.8, "attackDamage": 6,
                 "visionRadius": 8 },
    "Spitter": { "maxHP": 20,  "speed": 2.0, "scale": 0.9, "attackDamage": 8,
                 "visionRadius": 10, "behavior": "spitter", "tint": [190, 120, 235] }
  }
}
```
*— `engine-cpp/assets/data/enemy_variants.json` (extracto: 3 de 7 arquetipos)*

La carga es perezosa y cacheada en un `static` local inicializado por lambda —construcción bajo demanda, thread-safe por garantía del lenguaje desde C++11, y sin orden de inicialización global que razonar—:

```cpp
const std::unordered_map<std::string, EnemyFactory::EnemyVariant>& EnemyFactory::LoadVariants() {
    static const std::unordered_map<std::string, EnemyVariant> variants = [] {
        std::unordered_map<std::string, EnemyVariant> result;
        …
        variant.behavior = ParseBehavior(node.value("behavior", std::string("melee")));
        variant.tint = ParseTint(node);
        variant.turnRateDegPerSec = node.value("turnRateDegPerSec", 0.0f);
        …
        return result;
    }();
    return variants;
}
```
*— `engine-cpp/src/Entities/EnemyFactory.cpp`*

Y aquí está la decisión de acoplamiento que sostiene toda la parte 3: **el editor C# no tiene su propia copia de estos valores**. Los lee del mismo fichero, enlazado desde el árbol del motor:

```xml
<!-- Stats base por arquetipo, la misma fuente que usa EnemyFactory en el
     motor C++: enlazada, no duplicada, para que el editor no pueda
     desincronizarse de estos valores (ver Core/EnemyVariantCatalog.cs). -->
<None Include="..\engine-cpp\assets\data\enemy_variants.json"
      Link="Resources\Data\enemy_variants.json" CopyToOutputDirectory="PreserveNewest" />
```
*— `level-editor-csharp/LevelEditor.csproj`*

Gracias a eso, cuando en el editor se cambia la variante de un enemigo a `Tank`, sus campos de vida/velocidad/daño se rellenan con **los valores reales que usará el motor**, no con una tabla paralela que alguien tendría que acordarse de actualizar. Un dato duplicado entre dos proyectos es un *bug* con fecha de aparición diferida.

### 2.6 Consecuencia medible

El sistema tiene 15 niveles de campaña más el modo infinito. Ninguno de ellos aparece en el código: `kStoryLevelCount` es el único número que hay que tocar al añadir contenido, y existe solo porque el selector de niveles necesita saber cuántos botones pintar sin buscar en disco.

```cpp
// Cuántos level_<N>.json existen de verdad en assets/data. Es el tope de
// maxLevelUnlocked: sin él, superar el último nivel "desbloqueaba" uno
// más que no existe, y el selector le pintaba un botón que solo servía
// para rebotar al menú. Añadir un nivel nuevo implica subir esto.
static constexpr int kStoryLevelCount = 15;
```
*— `engine-cpp/src/Core/Application.h`*

Que este comentario exista —y que diga explícitamente qué hay que tocar— es la parte del diseño que evita que la única costura entre código y contenido se pudra en silencio.

---

## 3. Herramientas propias: interoperación con C# WinForms

### 3.1 La decisión: dos procesos, dos lenguajes, cero enlace

Colocar entidades escribiendo coordenadas a mano en un JSON no escala más allá del tercer nivel. Hacía falta un editor visual, y había tres caminos:

| Opción | Coste | Motivo del descarte |
|---|---|---|
| Editor **dentro** del motor (modo edición en raylib) | Reimplementar desde cero widgets, campos numéricos, diálogos de fichero, foco y teclado | Meses de trabajo para reconstruir lo que un toolkit de escritorio ya regala |
| Interoperación en proceso (C++/CLI, P/Invoke, incrustar WinForms en la ventana GL) | Un binario, dos runtimes | El bucle de render de raylib y el bucle de mensajes de WinForms **compiten por el hilo de UI**; además, un fallo del editor se lleva el juego |
| **Dos ejecutables, comunicación por fichero** | Un artefacto más que compilar | *Elegida* |

La elección se apoya en una observación sobre la naturaleza del acoplamiento: editor y motor **no necesitan comunicarse en tiempo real**. El flujo real de trabajo es asíncrono por definición —se edita, se guarda, se prueba—, así que el fichero JSON no es un sucedáneo de un canal IPC: **es el canal correcto**, y además persistente, inspeccionable, versionable en git y diffeable en una revisión.

Consecuencias directas: el editor puede evolucionar sin recompilar el motor; el motor no arrastra dependencias de .NET; un editor que casque no puede corromper el estado del juego; y cualquiera puede escribir un nivel con un editor de texto o generarlo con un script (§4).

### 3.2 El contrato

Toda la superficie de acoplamiento es el esquema JSON. Y como en cualquier contrato entre sistemas, lo interesante no es el caso feliz sino **la compatibilidad en las dos direcciones**.

Del lado C#, el esquema se declara con atributos sobre POCOs, no con serialización por convención:

```csharp
public class SpawnerData
{
    [JsonPropertyName("position")] public Vector3Data Position { get; set; } = new();
    [JsonPropertyName("enemyType")] public string EnemyType { get; set; } = "Runner";
    [JsonPropertyName("interval")]  public float Interval { get; set; } = 4.0f;
    [JsonPropertyName("maxEnemies")] public int MaxEnemies { get; set; } = 3;

    // Random Spawner: null (clave omitida) = spawner clásico, siempre
    // EnemyType. Con entradas, el motor sortea arquetipo en cada spawn
    // proporcionalmente al peso (ver Spawner::PickEnemyType).
    //
    // Nullable a propósito: el serializador omite las claves nulas (ver
    // LevelFileService.SaveOptions), así que un spawner normal sigue
    // exportándose exactamente igual que antes de existir esta mecánica.
    [JsonPropertyName("weights")] public Dictionary<string, int>? Weights { get; set; }

    public bool IsRandom => Weights is { Count: > 0 };
}
```
*— `level-editor-csharp/Models/SpawnerData.cs`*

El nombre serializado está **fijado explícitamente** y desacoplado del nombre C#, que sigue la convención `PascalCase` de su lenguaje. Sin el atributo, renombrar una propiedad —un refactor rutinario en C#— rompería silenciosamente el parseo del motor. El atributo convierte el nombre del campo en lo que realmente es: **parte de un contrato externo**, no un detalle de implementación.

Tres mecanismos concretos de compatibilidad, uno por cada dirección del problema:

**a) Escritura hacia atrás: omitir en vez de nulificar.**

```csharp
private static readonly JsonSerializerOptions SaveOptions = new()
{
    WriteIndented = true,
    // Si Door es null, se omite la clave "door" del JSON en vez de
    // escribir "door": null -- LevelLoader.cpp distingue "ausente"
    // (nivel sin puerta) de "presente pero null" (que le haría
    // fallar el parseo al intentar leer position/halfExtents).
    DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
};
```
*— `level-editor-csharp/Core/LevelFileService.cs`*

**b) Lectura defensiva del otro lado.** El motor no se fía de que esa opción esté siempre puesta:

```cpp
// !is_null() a propósito: System.Text.Json puede serializar una
// propiedad C# nula como `"door": null` en vez de omitir la clave.
// contains() por sí solo daría true y el parseo de abajo trabajaría
// sobre un null en vez de sobre un objeto.
if (root.contains("door") && root.at("door").is_object()) { … }
```
*— `engine-cpp/src/IO/LevelLoader.cpp`*

Los dos extremos defienden el mismo invariante desde su lado. Es redundante a propósito: el editor no es el único productor posible de estos ficheros (§4 los genera con scripts), así que el motor no puede asumir el comportamiento de un productor concreto.

**c) Migración de esquema en la lectura, no en el disco.** El formato de obstáculos cambió de `halfExtents` a `size`. En lugar de un script de migración masiva, el editor acepta ambos al leer y normaliza a uno solo al escribir:

```csharp
// Un nivel de antes de la fase de estructuras de entorno trae
// "halfExtents" en vez de "size" y no declara "type" -- se resuelve
// una vez al cargar y nunca se vuelve a escribir LegacyHalfExtents.
private static void MigrateLegacyObstacles(List<ObstacleData> obstacles)
{
    foreach (ObstacleData obstacle in obstacles)
    {
        if (obstacle.LegacyHalfExtents is Vector3Data legacy)
        {
            obstacle.Size = new Vector3Data(legacy.X * 2.0f, legacy.Y * 2.0f, legacy.Z * 2.0f);
            obstacle.LegacyHalfExtents = null;   // + WhenWritingNull => no se reescribe nunca
        }
    }
}
```
*— `level-editor-csharp/Core/LevelFileService.cs`*

Un fichero antiguo se convierte al abrirlo y sale al formato nuevo al guardarlo. La migración es un efecto secundario del uso normal, sin paso manual y sin ventana de incompatibilidad.

### 3.3 El editor por dentro: una arquitectura que no conoce al motor

`MainForm.cs` llegó a tener 1.760 líneas concentrando construcción de UI, herramienta activa, modelo del nivel, dibujado, panel de propiedades, localización y diálogos de fichero. Hoy tiene 819 y el resto vive en tres piezas con una regla de dependencia estricta:

```
                MainForm  (WinForms: controles, herramienta activa, diálogos)
                   │  posee            │ delega                │ delega
                   v                   v                       v
             EditorScene         CanvasRenderer         PropertyPanelBuilder
          (modelo del nivel)    (estático, sin estado)   (GroupBox + 3 delegados)
                   │                    │                       │
                   └──── ninguno de los tres conoce el motor C++ ────┘
```

**`EditorScene` — el modelo, sin una sola línea de WinForms.** Contiene las once listas del nivel y las operaciones que dependen *solo* de ellas: *hit-test*, borrado, reposicionamiento y traducción a/desde `LevelData`. No incluye `using System.Windows.Forms`, y esa ausencia es su especificación: **se puede razonar y probar sin levantar un formulario**.

```csharp
// Orden INVERSO al de dibujado (lo último pintado, arriba del todo, se
// prueba primero): Player, Enemies, Spawners, Barrels, PowerUps,
// HealthKits, Gears, Hazards, ElectricTiles, Door, Obstacles.
public object? FindAt(Point screenPoint)
{
    if (Player is not null && IsPointNearMarker(screenPoint, WorldToScreen(Player.Spawn)))
        return Player;

    object? marker = FindMarker(Enemies,     e => e.Spawn,    screenPoint)
                  ?? FindMarker(Spawners,    s => s.Position, screenPoint)
                  ?? FindMarker(Barrels,     b => b.Position, screenPoint)
                  …
    if (marker is not null) return marker;
    …
}
```
*— `level-editor-csharp/Core/EditorScene.cs`*

Que el *hit-test* recorra en orden inverso al de pintado no es un detalle: es la única forma de que seleccionar «lo que se ve encima» coincida con lo que el usuario percibe cuando dos entidades se superponen.

**`CanvasRenderer` — la vista, estática y sin estado.** Recibe la escena y qué hay seleccionado, y pinta. No puede mutar el modelo, y eso es una garantía estructural, no una convención:

```csharp
// Es estático y sin estado: recibe la escena y qué hay seleccionado, y
// pinta. Nada de lo que hay aquí puede modificar el nivel, que es
// justamente la garantía que se quería -- el dibujado no muta el modelo.
internal static class CanvasRenderer
{
    public static void Draw(Graphics g, EditorScene scene, object? selected, Font labelFont)
```
*— `level-editor-csharp/Canvas/CanvasRenderer.cs`*

**`PropertyPanelBuilder` — la UI dinámica.** Es la pieza que responde a la pregunta «cómo se construye una interfaz agnóstica»: el panel de propiedades no está diseñado en ningún fichero de diseñador ni existe como jerarquía de controles precreada. Se **genera en tiempo de ejecución** a partir del *tipo* de la entidad seleccionada.

### 3.4 UI generada por tipo, sin acoplarse a nada

El punto de entrada es un despacho por tipo sobre un `object?`. C# moderno lo expresa con *pattern matching*, incluida una guarda para el caso en que un mismo tipo tenga dos presentaciones (`box` frente a `cylinder`):

```csharp
private void Rebuild()
{
    _group.Controls.Clear();
    _setHeight(DefaultPropertiesHeight);

    switch (_entity)
    {
        case PlayerData player:  _group.Visible = true; BuildPlayerProperties(player); break;
        case EnemyData enemy:    _group.Visible = true; BuildEnemyProperties(enemy);   break;

        case ObstacleData obstacle when obstacle.Type == "cylinder":
            _group.Visible = true; BuildCylinderProperties(obstacle); break;
        case ObstacleData obstacle:
            _group.Visible = true; BuildObstacleBoxProperties(obstacle); break;
        …
        default:
            // Gear seleccionado (sin propiedades editables todavía,
            // solo posición), o nada seleccionado.
            _group.Visible = false; break;
    }
}
```
*— `level-editor-csharp/UI/PropertyPanelBuilder.cs`*

Cada constructor de panel se reduce a una declaración de filas. Toda la mecánica —crear el control, situarlo, traducir la etiqueta, escribir en el modelo, marcar el fichero como sucio, repintar si hace falta— vive en un único helper:

```csharp
private void BuildPlayerProperties(PlayerData player)
{
    AddRow("prop_hp",     0, 1,    1000, 0, 1,    player.MaxHP,        v => player.MaxHP = v);
    AddRow("prop_speed",  1, 0.5m, 20,   1, 0.5m, player.Speed,        v => player.Speed = v);
    AddRow("prop_damage", 2, 1,    500,  0, 1,    player.AttackDamage, v => player.AttackDamage = v);
}
```
*— `level-editor-csharp/UI/PropertyPanelBuilder.cs`*

```csharp
// Una fila del panel: su etiqueta traducida y el campo numérico que
// escribe en el modelo. Los veinte campos solo se diferencian en
// fila, rango, decimales, paso y qué asignan, así que se arman todos
// por aquí.
//
// repaint: para lo que cambia la HUELLA EN PLANTA de la entidad
// (ancho/largo, radio, el intervalo que va escrito sobre una baldosa)
// y por tanto obliga a repintar el lienzo. El alto (Y) no se ve desde
// arriba, así que no lo pide.
private void AddRow(string textKey, int row, decimal min, decimal max, int decimals, decimal step,
                    float value, Action<float> apply, bool repaint = false)
{
    // El valor se recorta al rango del control: un JSON externo puede
    // traer un enemigo sin "maxHP" (llega como 0, y el mínimo es 1) y
    // NumericUpDown lanza ArgumentOutOfRangeException al asignarlo,
    // lo que se llevaría el editor por delante al seleccionarlo.
    var input = new NumericUpDown
    {
        Location = new Point(10, InputY(row)), Width = 160,
        Minimum = min, Maximum = max, DecimalPlaces = decimals, Increment = step,
        Value = Math.Clamp((decimal)value, min, max)
    };
    input.ValueChanged += (s, e) =>
    {
        apply((float)input.Value);
        _markDirty();
        if (repaint) _invalidateCanvas();
    };

    _group.Controls.Add(CreateLabel(textKey, new Point(10, LabelY(row))));
    _group.Controls.Add(input);
}
```
*— `level-editor-csharp/UI/PropertyPanelBuilder.cs`*

El `Math.Clamp` merece una nota, porque es exactamente el mismo principio de §2.2 aplicado al otro lado del contrato: **la entrada es hostil también aquí**. `NumericUpDown` lanza si el valor asignado cae fuera de `[Minimum, Maximum]`; un nivel escrito a mano sin la clave `"maxHP"` deserializa el enemigo con 0, el mínimo del control es 1, y el editor moría al *seleccionar* esa entidad. El editor consume ficheros que no ha producido él, así que necesita la misma tolerancia que el motor.

Y el desacoplamiento estructural está en la firma del constructor:

```csharp
public PropertyPanelBuilder(GroupBox group, Font baseFont, Action markDirty,
                            Action invalidateCanvas, Action<int> setHeight)
```
*— `level-editor-csharp/UI/PropertyPanelBuilder.cs`*

Recibe **un contenedor y tres efectos**, no un formulario. Marcar el fichero como modificado, repintar el lienzo y recolocar lo que va debajo del panel son cosas que ocurren fuera de su `GroupBox`, y entran como delegados. El resultado es que el fichero **no sabe que existe un `MainForm`** y que, en el arnés de pruebas, se instancia contra un `GroupBox` desnudo con tres lambdas que se limitan a contar llamadas.

### 3.5 Verificar una GUI sin GUI

La consecuencia práctica de esa arquitectura es que el editor se puede verificar sin abrir ninguna ventana ni automatizar ningún ratón. El arnés hace tres cosas, en un hilo `STA`:

1. **Ida y vuelta del contrato** sobre los 16 niveles reales: fichero → `EditorScene.LoadFrom` → `ToLevelData` → fichero → `LoadFrom`, comparando una firma que incluye conteos, tipos, tamaños y tablas de pesos. Comprueba también que un spawner clásico **no** emite la clave `"weights"` (§3.2a).
2. **Construcción completa del formulario**: `new MainForm()` ejecuta el constructor entero —incluida la pasada de localización que reconstruye el panel— y `Dispose()` lo cierra, sin que nada aparezca en pantalla.
3. **Un panel por cada tipo de entidad**, comprobando visibilidad y número de controles; y sobre un enemigo real cargado del nivel 1, que cada campo muestre el valor del modelo, que editarlo lo escriba de vuelta y marque el fichero como sucio, y que el alto de un obstáculo **no** repinte el lienzo mientras el ancho **sí**.

```
== Round-trip por EditorScene ==
OK  level_1.json  (2 enemigos, 10 obstaculos, 0 spawners, 0 baldosas, 0 powerups)
…
== MainForm + PropertyPanelBuilder ==
OK  MainForm construido
OK  Enemy          visible=True controles=10 alto=300
OK  Spawner random visible=True controles=21 alto=416
OK  enemigo real   valores 70/5/3,5/10 -> editar escribe en el modelo y marca sucio
OK  obstaculo      el Alto no repinta el lienzo y el Ancho si
TODO OK
```

Cubre casi todo lo que cubriría el *click-testing* manual, corre en un segundo y no depende de que la máquina esté libre. Esa segunda propiedad —una comprobación que no compite por el ratón ni el teclado de nadie— resultó valer más de lo previsto.

### 3.6 El único punto de contacto en ejecución

Los dos procesos se tocan una sola vez, y en una sola dirección:

```cpp
// Idioma actual como argumento posicional (args[0] en Program.cs) --
// así el editor abre ya en el mismo idioma que el juego en vez de
// arrancar siempre en el último que se usó dentro del propio editor.
std::string command = std::string("start \"\" \"") + kEditorPath + "\" "
                    + m_localization.GetCurrentLanguage();
std::system(command.c_str());
```
*— `engine-cpp/src/Core/Application.cpp`*

```csharp
// args[0] es el código de idioma que pasa el motor C++ al lanzar este
// editor (ver Application::LaunchLevelEditor). Sin argumento (abierto a
// mano, doble clic), se mantiene el comportamiento de siempre: el idioma
// guardado en editor_settings.json. LocalizationManager.SetLanguage ya cae
// a "es" por su cuenta si el código no es reconocido, así que un argumento
// inesperado no puede romper el arranque.
string initialLanguage = args.Length > 0 ? args[0] : EditorSettings.Load().Language;
```
*— `level-editor-csharp/Program.cs`*

Un proceso hijo desacoplado y **un** argumento de línea de órdenes. Merece señalarse lo que este acoplamiento mínimo *ya* tiene bien resuelto: el editor sigue siendo ejecutable de forma autónoma —el argumento es opcional y hay un valor de respaldo persistido—, y un valor inesperado degrada a un idioma por defecto en lugar de impedir el arranque. Es el mismo criterio de §2.4, aplicado a un contrato de una sola cadena.

Las limitaciones también son explícitas en el código: la ruta al editor es relativa al directorio de trabajo del juego y solo resuelve en la máquina de desarrollo con ambos proyectos compilados. Es una decisión consciente de un proyecto de portfolio, documentada donde está el código, no una omisión.
---

## 4. Algoritmia aplicada: validación de niveles por flood-fill

### 4.1 El problema: un JSON válido no es un nivel jugable

Los niveles 11 a 15 se generaron con scripts, y el 7 es un laberinto. En cuanto un nivel deja de escribirse a mano aparece una clase de fallo que ninguna herramienta del pipeline detecta:

- el JSON es **sintácticamente** válido;
- el esquema es **semánticamente** válido (todos los campos, todos los tipos);
- el `LevelLoader` lo carga sin un solo aviso;
- el nivel se dibuja perfectamente;
- y es **imposible de terminar**, porque un muro sella la única ruta hacia la puerta.

El caso real que originó la herramienta: la primera versión del laberinto del nivel 7, escrita a ojo colocando muros coordenada a coordenada, dejó el punto de aparición del jugador en una bolsa cerrada por cuatro muros. Puerta y cinco engranajes, inalcanzables. Leyendo el JSON no se ve —son 68 obstáculos con sus coordenadas— y un *playtest* tarda minutos en descubrirlo, si es que el tester intenta ir por el lado correcto.

Es, en el fondo, un problema clásico de **conectividad en un grafo**, disfrazado de fichero de datos.

### 4.2 Del problema continuo al discreto

El espacio de juego es continuo (`float` en el plano XZ) y el jugador es una caja alineada a los ejes, no un punto. Reducirlo a un grafo requiere tres transformaciones, y **cada una tiene que elegir el lado seguro del error**: es preferible que el validador declare inalcanzable algo que sí lo es (falso positivo, cuesta una revisión manual) a que dé por bueno un nivel roto (falso negativo, llega al jugador).

**a) Suma de Minkowski: el jugador pasa a ser un punto.** En vez de comprobar la colisión de una caja de 1×1 contra cada obstáculo, se **infla** cada obstáculo con el medio-tamaño del jugador. El problema «¿cabe la caja aquí?» se convierte en «¿está este punto dentro de algún rectángulo?».

```
   obstáculo real          obstáculo inflado (+0.5 por lado)
   ┌───────┐               ╔═══════════╗
   │       │      ═══>     ║ ┌───────┐ ║   y el jugador, un punto
   └───────┘               ║ └───────┘ ║
                           ╚═══════════╝
```

**b) Rejilla de paso 0.25.** Muestrear introduce el riesgo de «atravesar» una pared más fina que el paso. Aquí no puede ocurrir, y el argumento es aritmético: el editor no permite crear un obstáculo de menos de 0.2 unidades de grosor y la inflación le suma 1.0, así que **ninguna región bloqueada puede medir menos de 1.2** — casi cinco veces el paso (el muro más fino que hay hoy en los datos mide 0.6, es decir, 1.6 ya inflado). Análogamente, dos celdas libres contiguas tienen siempre libre el segmento que las une, así que la conectividad de la rejilla implica conectividad real.

**c) Cuatro vecinos, no ocho.** Es la transformación más fácil de equivocar. El motor resuelve el movimiento **eje a eje** (`Entity::TryMove` prueba X, y luego Z desde la X ya resuelta), así que el jugador no tiene un movimiento diagonal atómico: una diagonal solo es transitable si al menos uno de sus dos caminos en «L» lo es, y ese camino ya lo cubre la conectividad ortogonal. Con 8 vecinos el validador se inventaría pasos entre dos esquinas que se tocan y por las que el jugador real no pasa: **un falso negativo, justo el error que no se puede permitir**.

Un último detalle de fidelidad: los pilares cilíndricos se validan como **cuadrados**, no como círculos, porque eso es exactamente lo que el motor colisiona (`Cylinder` usa el AABB genérico de `Entity`). Validar con un modelo más fino que el del juego daría por buenos huecos por los que el jugador no cabe.

> **Principio general.** Un validador debe replicar el modelo de colisión del motor, no el modelo geométrico «correcto». Cualquier discrepancia entre ambos es una fuente de falsos veredictos en las dos direcciones.

### 4.3 El algoritmo

La herramienta vive en `engine-cpp/tools/validate_levels.py`. Es una herramienta de **autoría**: no se compila con el motor, no se distribuye con el juego, y se ejecuta al generar o tocar un nivel.

Constantes tomadas del motor, no elegidas:

```python
# Actor: halfExtents por defecto {0.5, 0.5, 0.5} -> el jugador es una caja de
# 1x1 en el plano XZ (ver Entities/Actor.h y Entity::CollidesWithAny).
PLAYER_HALF = 0.5

# Perimetro de la arena: muros en +-16, interior jugable +-15.
ARENA_LIMIT = 16.5

# Paso de la rejilla. Tiene que ser MENOR que el grosor minimo de un
# obstaculo YA INFLADO, para que el flood-fill no pueda "atravesar" una
# pared saltandosela entre dos muestras. Cota inferior: el editor no deja
# crear un obstaculo de menos de 0.2, y la inflacion suma 1.0 -> 1.2 (el
# mas fino que hay hoy en los datos mide 0.6 -> 1.6).
STEP = 0.25
```

Construcción del conjunto de bloqueadores (paso **a** de §4.2):

```python
def build_blockers(level):
    """Obstaculos como AABB en XZ ya INFLADOS con el medio-tamano del jugador
    (suma de Minkowski): asi el jugador se reduce a un punto y basta con
    preguntar si el punto cae dentro de alguna caja.

    La Y se ignora a proposito: todos los obstaculos del proyecto tienen
    size.y = 3 centrado en y = 0, y la caja del jugador ([-0.5, 0.5]) siempre
    los solapa en ese eje. Reintroducirla no cambiaria ni un resultado y
    ocultaria que el problema es bidimensional.
    """
    boxes = []
    for o in level.get("obstacles", []):
        px, _, pz = vec(o, "position")
        if o.get("type") == "cylinder":
            # El motor NO usa el circulo: colisiona contra el cuadrado
            # circunscrito (ver Cylinder.h). Se replica tal cual.
            r = float(o.get("radius", 0.5))
            hx = hz = r
        else:
            sx, _, sz = vec(o, "size", (1.0, 1.0, 1.0))
            hx, hz = sx * 0.5, sz * 0.5
        boxes.append((px, pz, hx + PLAYER_HALF, hz + PLAYER_HALF))
    return boxes


def blocked(x, z, boxes):
    """AABBIntersects del motor es INCLUSIVO (tocarse cuenta como chocar),
    de ahi los <= en vez de <."""
    for cx, cz, hx, hz in boxes:
        if abs(x - cx) <= hx and abs(z - cz) <= hz:
            return True
    return False
```

El recorrido es un BFS con cola —una anchura primero sobre la rejilla implícita—. Se prefiere a la recursión de un DFS por una razón puramente práctica: la región conectada de un nivel abierto ronda las 12.000 celdas y una implementación recursiva desbordaría la pila del intérprete. La estructura del grafo es la misma; solo cambia el orden de visita, que aquí es irrelevante porque interesa el **conjunto alcanzable**, no los caminos.

```python
def flood(level, boxes):
    """BFS de 4 vecinos desde el spawn del jugador sobre la rejilla de
    celdas libres.

    4 vecinos, no 8: el motor resuelve X y Z por separado (Entity::TryMove),
    asi que una diagonal solo es transitable si al menos uno de sus dos
    caminos en L lo es -- y ese camino en L ya lo cubre la conectividad
    ortogonal. Con 8 vecinos el validador se inventaria pasos en diagonal
    entre dos esquinas que el jugador no puede cruzar.
    """
    sx, _, sz = vec(level["player"], "spawn")
    start = to_cell(sx, sz)

    limit = int(ARENA_LIMIT / STEP)
    visited = set()

    if blocked(start[0] * STEP, start[1] * STEP, boxes):
        return visited, False          # el spawn en si esta dentro de un muro

    queue = deque([start])
    visited.add(start)
    while queue:
        cx, cz = queue.popleft()
        for dx, dz in ((1, 0), (-1, 0), (0, 1), (0, -1)):
            nx, nz = cx + dx, cz + dz
            if (nx, nz) in visited or abs(nx) > limit or abs(nz) > limit:
                continue
            if blocked(nx * STEP, nz * STEP, boxes):
                continue
            visited.add((nx, nz))
            queue.append((nx, nz))
    return visited, True
```

Y el veredicto: cada objetivo —engranajes, botiquines, power-ups y la puerta— se busca en la región alcanzada, con una tolerancia de una celda porque el jugador recoge por **solapamiento de cajas**, no pisando el centro exacto del objeto:

```python
def reachable(point, visited, boxes):
    """Un objetivo cuenta como alcanzable si alguna celda de su entorno
    inmediato (+-1 celda) esta en la region visitada."""
    x, z = point
    cx, cz = to_cell(x, z)
    for dx in (-1, 0, 1):
        for dz in (-1, 0, 1):
            if (cx + dx, cz + dz) in visited:
                return True
    return False
```

### 4.4 La segunda comprobación: entidades embebidas

El flood-fill por sí solo no detecta un objeto **dentro** de un muro: si el muro linda con zona libre, el objeto queda a menos de una celda de una celda visitada y se declara alcanzable. Pero no lo es: es contenido perdido. Hace falta una comprobación independiente, y esta vez contra la caja **sin inflar** —el objeto no es el jugador—:

```python
def embedded(level, boxes):
    """Entidades clavadas DENTRO de un obstaculo. El flood-fill por si solo
    no las caza: una baldosa o un engranaje dentro de un muro sigue
    'alcanzable' si el muro toca zona libre, pero es contenido perdido.
    """
```

Los tres fallos que este par de comprobaciones cazó en los niveles 11–15 recién generados, y que ninguna revisión visual del JSON había detectado:

| Nivel | Fallo | Detectado por |
|---|---|---|
| 11 | Baldosa eléctrica clavada dentro de un muro | `embedded` |
| 13 | Pilar encima del punto de aparición del jugador → **nivel entero inalcanzable** | `flood` (spawn bloqueado) |
| 14 | Baldosa dentro de un muro, y los vanos laterales no eran vanos de verdad | ambas |

### 4.5 Salida real sobre los 16 ficheros del proyecto

```
OK  endless.json     12 objetivos alcanzables,  680.9 u2 de suelo conectado (10895 celdas)
OK  level_1.json      3 objetivos alcanzables,  798.4 u2 de suelo conectado (12775 celdas)
OK  level_7.json     10 objetivos alcanzables,  430.6 u2 de suelo conectado (6889 celdas)
OK  level_13.json    12 objetivos alcanzables,  783.6 u2 de suelo conectado (12538 celdas)
OK  level_15.json    19 objetivos alcanzables,  695.5 u2 de suelo conectado (11128 celdas)
…
TODO OK
```

La métrica de superficie conectada es un subproducto gratuito del algoritmo y resultó ser un indicador de diseño útil: el nivel 7 (el laberinto) expone **430 u² de 800**, aproximadamente la mitad que un nivel abierto, que es exactamente lo que se espera de un laberinto y no de un nivel que se ha sellado por accidente. Una caída brusca de esta cifra entre dos versiones de un mismo nivel es señal de que un muro nuevo ha cerrado algo.

**Control negativo.** Un verificador que solo se ha visto decir «OK» no está verificado: puede estar devolviendo `True` por construcción. Se comprobó inyectando los dos fallos que debe cazar sobre una copia del nivel 11 —cuatro muros sellando el punto de aparición, y un engranaje colocado dentro del muro perimetral—:

```
FALLO level_sellado.json: gears[0] en (-12.0, 6.0) es INALCANZABLE desde el spawn
FALLO level_sellado.json: door en (0.0, -14.5) es INALCANZABLE desde el spawn
…
FALLO level_gear_en_muro.json: gears[5] en (16.0, 0.0) es INALCANZABLE desde el spawn
FALLO level_gear_en_muro.json: gears[5] en (16.0, 0.0) dentro de un obstaculo
```

### 4.6 La otra mitad: garantizar por construcción

Validar *a posteriori* dice si un nivel está roto, pero no ayuda a producir uno correcto. Para el laberinto del nivel 7 se usó el enfoque complementario: **generarlo con un algoritmo cuya salida es conectada por construcción**.

Un **laberinto perfecto** —exactamente un camino entre cada par de celdas, sin ciclos— es un árbol de expansión del grafo de salas. Generarlo por DFS con retroceso garantiza la propiedad sin necesidad de comprobarla:

```
GenerarLaberinto(salas 5x5, semilla fija):
    pila <- [sala inicial];  visitadas <- {sala inicial}
    mientras la pila no esté vacía:
        actual <- cima(pila)
        vecinos <- vecinos de 'actual' no visitados
        si vecinos está vacío:
            desapilar                          # retroceso
        si no:
            siguiente <- elegir(vecinos)        # con la semilla fija
            derribar el muro entre actual y siguiente
            marcar 'siguiente' como visitada; apilar
```

Dos decisiones de implementación con consecuencias reales:

- **Semilla fija.** Un nivel de campaña tiene que ser el mismo en cada ejecución del generador: es contenido versionado, no contenido procedural en tiempo de juego. La semilla convierte el generador en una función pura y hace el nivel **reproducible** y su diff en git, interpretable.
- **Cada frontera abierta se parte en dos muros**, dejando un vano de 2 unidades en el centro. Un vano tiene que ser mayor que el ancho del jugador (1 unidad) más el margen de la colisión inclusiva; un hueco de exactamente 1 unidad sería geométricamente un paso e ingame un muro.

Aun generándolo así, el resultado **se pasa igualmente por el validador**. Las garantías por construcción cubren la topología de salas, no el resto del contenido: los engranajes, los power-ups y la puerta se colocan después, y nada impide colocarlos dentro de un muro. Generación y validación son complementarias, no alternativas.

### 4.7 Complejidad y límites

Con `n` celdas de la rejilla (133×133 = 17.689 en este proyecto) y `m` obstáculos:

| | Coste | Nota |
|---|---|---|
| `blocked(x, z)` | *O(m)* | Barrido lineal de rectángulos. Con `m ≤ 68`, indexarlo espacialmente no compensa |
| `flood` | *O(n · m)* | Cada celda se encola una vez; 4 comprobaciones de vecino |
| Total, 16 niveles | ≈ 1 s | Frente a los minutos de un *playtest* que además no garantiza cobertura |

Límites conocidos, declarados para que nadie lea de más en un «TODO OK»:

- Valida **conectividad geométrica**, no jugabilidad. Un pasillo cubierto de baldosas eléctricas es atravesable para el validador y letal para el jugador. Es intencionado: mezclar ambas cosas exigiría simular combate.
- No modela entidades móviles: los enemigos no bloquean el paso en este juego, así que la topología es estática.
- Los objetivos que aparecen en tiempo de ejecución —los power-ups que sueltan los enemigos al morir— caen fuera por definición: no están en el fichero.

Aun con esos límites, cubre el modo de fallo que importa: **un nivel imposible de completar que parece perfectamente correcto en todas las capas anteriores**.

---

## 5. Apéndices

### A. Índice de ficheros citados

| Fichero | Papel |
|---|---|
| `engine-cpp/src/Core/Application.{h,cpp}` | Bucle, estados de aplicación, orden de destrucción, resolución de cascadas |
| `engine-cpp/src/Core/CountdownTimer.h` | Temporizador descendente compartido (y su frontera documentada) |
| `engine-cpp/src/Entities/Entity.{h,cpp}` | Base polimórfica, AABB, movimiento eje a eje |
| `engine-cpp/src/Entities/ExplosiveBarrel.h` | Patrón «consumir» para cascadas |
| `engine-cpp/src/Entities/Spawner.{h,cpp}` | Punteros observadores y su invariante |
| `engine-cpp/src/Entities/EnemyFactory.cpp` | Catálogo de arquetipos data-driven |
| `engine-cpp/src/IO/LevelLoader.{h,cpp}` | Parseo tolerante, `ParseArray`, `LevelData` |
| `engine-cpp/src/Renderer/ShaderManager.{h,cpp}` | Wrapper RAII canónico |
| `engine-cpp/src/Renderer/ModelUtils.cpp` | Liberación de texturas propias (raylib 6.0) |
| `engine-cpp/tools/validate_levels.py` | Validador de conectividad (herramienta de autoría) |
| `level-editor-csharp/Core/EditorScene.cs` | Modelo del nivel, sin WinForms |
| `level-editor-csharp/Core/LevelFileService.cs` | Serialización y migración de esquema |
| `level-editor-csharp/Canvas/CanvasRenderer.cs` | Vista estática sin estado |
| `level-editor-csharp/UI/PropertyPanelBuilder.cs` | UI generada por tipo en tiempo de ejecución |
| `level-editor-csharp/Models/*.cs` | Declaración del contrato JSON |

### B. Constantes de contrato

| Constante | Valor | Dónde | Consecuencia de cambiarla |
|---|---|---|---|
| `kStoryLevelCount` | 15 | `Application.h` | Hay que subirla al añadir un `level_<N>.json` |
| Perímetro de arena | ±16 (interior ±15) | Los 16 JSON + `DrawGroundGrid` (32 divisiones) + `CanvasSize`/`CellSize` del editor | Tres sitios a la vez: motor, datos y editor |
| `PLAYER_HALF` | 0.5 | `Actor` (defecto) y el validador | Cambia qué huecos son atravesables |
| `STEP` | 0.25 | Validador | Debe seguir siendo < 1.2 (§4.2b) |
| `kExplosionRadius` / `kExplosionDamage` | 3.5 / 50 | `ExplosiveBarrel.h` | Mecánica, no contenido: constante a propósito |

### C. Cómo compilar y verificar

```bash
# Motor C++ (Debug y Release, /W4, 0 warnings como estándar del proyecto)
cmake -S engine-cpp -B engine-cpp/build
cmake --build engine-cpp/build --config Debug
cmake --build engine-cpp/build --config Release

# Editor C#
dotnet build level-editor-csharp/LevelEditor.csproj -c Debug
dotnet build level-editor-csharp/LevelEditor.csproj -c Release

# Validación de contenido: conectividad de los 16 niveles
python engine-cpp/tools/validate_levels.py
```

> **Nota sobre `CMakeLists.txt`.** Al añadir o quitar un fichero fuente, la primera compilación posterior puede fallar: MSBuild regenera el `.vcxproj` **a mitad de esa misma invocación**, cuando ya había cargado la lista de ficheros antigua. No es un error del código; se resuelve relanzando la compilación sin tocar nada.
