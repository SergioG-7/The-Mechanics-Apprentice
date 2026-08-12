# engine-cpp — Paso 1: ventana + modelo + shader básico

## Requisitos
- Visual Studio 2022 con el workload "Desarrollo para el escritorio con C++" (incluye CMake y el compilador MSVC).
- Conexión a internet la primera vez que configures el proyecto (CMake descarga raylib y nlohmann/json vía FetchContent — no hace falta instalar nada a mano ni usar vcpkg).

## Abrir el proyecto en Visual Studio 2022
1. Archivo → Abrir → Carpeta… y selecciona la carpeta `engine-cpp/` (la que contiene `CMakeLists.txt`), no la carpeta raíz `level5-portfolio/`.
2. VS detecta el `CMakeLists.txt` automáticamente y empieza a generar la caché de CMake (barra de estado abajo: "Generando caché de CMake…"). La primera vez tarda 1–3 minutos porque descarga y compila raylib desde GitHub.
3. Cuando termine, en la barra de herramientas superior el selector de configuración debe mostrar algo como `x64-Debug`, y a su lado el selector de "elemento de inicio" debe mostrar `Level5Portfolio.exe`. Si pone "Seleccionar elemento de inicio", despliega el menú y elígelo ahí.
4. Pulsa **Play (▶ Level5Portfolio.exe)** o F5.

## Assets y shaders — no hay que moverlos a mano
El `CMakeLists.txt` ya copia `shaders/` y `assets/` junto al `.exe` en cada build
(`add_custom_command ... POST_BUILD`). Y el "working directory" con el que VS
ejecuta el binario al pulsar Play es, por defecto, la carpeta del propio `.exe`
— así que las rutas relativas del código (`"shaders/toon.vs"`, `"assets/test_model.glb"`)
se resuelven solas. No necesitas tocar ninguna configuración de depuración.

Lo único pendiente de tu parte: coloca un modelo 3D real en
`engine-cpp/assets/test_model.glb` (ver `assets/README.md`). Sin él, la ventana
y el shader funcionan igual, pero `GameModel` te avisará por consola
("failed to load") y no verás nada en el centro de la escena.

## Qué hace este paso
- Abre una ventana 1280x720, cámara orbital sobre el origen.
- Carga el modelo con una clase RAII (`GameModel`) que garantiza `UnloadModel` una sola vez.
- Compila `shaders/toon.vs` + `shaders/toon.fs` (cel-shading con 4 bandas de luz) y lo aplica
  a todos los materiales del modelo.
- `Application` es la única clase que toca el ciclo de vida de la ventana de raylib; todo lo
  demás recibe recursos ya inicializados.

## Si algo falla
- **"CMake Error… could not find raylib/json"**: revisa tu conexión — FetchContent necesita
  llegar a `github.com` en la primera configuración.
- **Pantalla negra sin modelo**: falta `assets/test_model.glb`, es esperado (ver arriba).
- **Quieres forzar el working directory manualmente**: clic derecho sobre
  `Level5Portfolio.exe` en el árbol de CMake (panel "Vista de carpeta") →
  "Depurar y configuraciones de inicio" → genera `launch.vs.json`, donde puedes
  fijar `"currentDir"`. No debería hacer falta con el `POST_BUILD` ya incluido.
