# LEVEL-5 Portfolio Project: Action-RPG Engine & Tooling


Un motor de Action-RPG 3D hecho desde cero en C++ y una herramienta de creación de niveles personalizada (Custom Level Editor) en C#. Este proyecto ha sido diseñado como prueba técnica y muestra de portfolio, enfocado en las tecnologías base, el *scope management* y las filosofías de diseño de LEVEL-5.

## 🛠️ Tecnologías Utilizadas

*   **Core / Engine:** C++17
*   **Framework Gráfico:** Raylib 6.0
*   **Gestión de Dependencias y Build:** CMake (FetchContent)
*   **Serialización de Datos:** nlohmann/json
*   **Tooling (Editor de Niveles):** C# / WinForms (.NET)

## ⚙️ Arquitectura y Características Clave

1.  **Arquitectura Data-Driven & Tooling:** El motor no tiene datos *hardcodeados*. Se ha desarrollado un Editor de Niveles propio en C# que permite diseñar la arena, colocar enemigos y trazar rutas de patrulla. Este editor exporta un archivo `.json` que el motor C++ parsea de forma robusta (`LevelLoader.cpp`) para instanciar el nivel dinámicamente.
2.  **FSM (Finite State Machines):** Lógica de entidades controlada por máquinas de estados limpias (`IDLE`, `RUN`, `DASH`, `ATTACK`, `PATROL`, `CHASE`, `HURT`, `DEAD`), separando claramente el comportamiento del jugador y de la Inteligencia Artificial.
3.  **Gestión de Memoria (RAII):** Uso estricto de RAII para la gestión de recursos de la tarjeta gráfica (VRAM). Los modelos 3D y las texturas se cargan y liberan automáticamente en los constructores y destructores de las entidades, previniendo fugas de memoria (*memory leaks*).
4.  **Combate y Hitboxes:** Sistema de combate basado en cajas delimitadoras alineadas a los ejes (AABB), hitboxes temporales y cálculo de *Knockback* (retroceso) con físicas vectoriales normalizadas.
5.  **Scope Management & Arte:** Para mantener el código ligero y centrado en la arquitectura, se optó por un estilo visual minimalista (tipo "Simulador VR" o "Sci-Fi"). Los modelos `.gltf` se renderizan utilizando primitivas 3D y tintes sólidos, demostrando capacidad para priorizar rendimiento y lógica sobre dependencias gráficas complejas.

## 📖 Lore y Diseño

*   **El Protagonista:** Un joven aprendiz de mecánico (Tintado en azul). Cuenta con un sistema de movimiento fluido (Dash con cooldown) y ataques cuerpo a cuerpo.
*   **Los Enemigos:** Robots de seguridad averiados (Tintados en rojo). Cuentan con rutinas de patrulla personalizadas, conos de visión (distancia al jugador) y persecución. Al vaciar su HP, se "desactivan" (estado `DEAD`).
*   **Objetivo:** Recolectar engranajes en el entorno y alcanzar la zona de extracción (puerta).

## 🚀 Cómo Compilar (Visual Studio 2022)

1. Abre Visual Studio 2022.
2. Selecciona `Archivo` -> `Abrir` -> `Carpeta...`
3. Selecciona la carpeta `engine-cpp`.
4. Visual Studio detectará `CMakeLists.txt` y descargará las dependencias automáticamente (Raylib y JSON).
5. Selecciona `Level5Portfolio.exe` como elemento de inicio y pulsa `F5`.

*(Nota: Para crear nuevos niveles, compila y ejecuta el proyecto `level-editor-csharp` de la carpeta raíz (o ábrelo desde el propio juego con el botón "Editor de Niveles" del menú principal / F12), diseña tu nivel y expórtalo a `engine-cpp/assets/data/level_<N>.json` o `engine-cpp/assets/data/endless.json` -- son los únicos archivos que el motor C++ carga.)*