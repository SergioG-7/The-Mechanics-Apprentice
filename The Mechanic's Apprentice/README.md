# LEVEL-5 Portfolio Project: Action-RPG Engine & Tooling

Un motor de Action-RPG 3D hecho desde cero en C++ (con Raylib) y una herramienta de creación de niveles Data-Driven (en desarrollo). Este proyecto fue diseñado como prueba técnica y muestra de portfolio, enfocado en las tecnologías base y filosofías de diseño de LEVEL-5.

## 🛠️ Tecnologías Utilizadas

*   **Core / Engine:** C++17
*   **Framework Gráfico:** Raylib 6.0
*   **Gestión de Dependencias y Build:** CMake (FetchContent)
*   **Serialización de Datos:** nlohmann/json 3.12.0
*   **Tooling (Próximamente):** C# / WinForms

## ⚙️ Arquitectura y Características Clave

1.  **Arquitectura Data-Driven:** Los niveles, enemigos, rutas de patrulla y parámetros (HP, radios de visión) no están *hardcodeados*. Se cargan dinámicamente desde archivos `.json` mediante un parser robusto en `LevelLoader.cpp`.
2.  **Gestión de Memoria (RAII):** Uso estricto de RAII para la gestión de recursos de la GPU. Clases como `GameModel` y `ShaderManager` previenen fugas de memoria (memory leaks) sin necesidad de llamadas manuales a destructores explícitos fuera de su scope.
3.  **FSM (Finite State Machines):** Lógica de entidades controlada por máquinas de estados limpias (`IDLE`, `RUN`, `ATTACK`, `PATROL`, `CHASE`, `HURT`, `DEAD`), separando claramente el comportamiento de IA y jugador.
4.  **Combate y Hitboxes:** Sistema de combate basado en cajas delimitadoras alineadas a los ejes (AABB) y hitboxes temporales.
5.  **Shaders Personalizados (GLSL):** Implementación de un Toon Shader (Cel-shading) para replicar el estilo visual característico de los juegos de LEVEL-5.

## 📖 Lore y Diseño de Producto

Para demostrar sensibilidad hacia el diseño de juegos, el proyecto cuenta con un pequeño trasfondo:
*   **El Protagonista:** Un joven aprendiz de mecánico. Su ataque es un barrido contundente con una llave inglesa gigante.
*   **Los Enemigos:** Juguetes mecánicos o robots averiados. Al vaciar su barra de HP, no mueren, simplemente se "desactivan" (estado `DEAD`).
*   **Objetivo:** Recolectar engranajes y sobrevivir en la arena.

## 🚀 Cómo Compilar (Visual Studio 2022)

1.  Abre Visual Studio 2022.
2.  Selecciona `Archivo` -> `Abrir` -> `Carpeta...`
3.  Selecciona la carpeta `engine-cpp`.
4.  Visual Studio detectará `CMakeLists.txt` y descargará las dependencias automáticamente (Raylib y JSON).
5.  Selecciona `Level5Portfolio.exe` como elemento de inicio y pulsa `F5`.

*(Nota: Asegúrate de tener un archivo `test_model.glb` u `.obj` en la carpeta `assets/` para evitar avisos de carga fallida).*
