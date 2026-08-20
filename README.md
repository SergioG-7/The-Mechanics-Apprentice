# ⚙️ The Mechanic's Apprentice

[Español](#-español) | [English](#-english) | [日本語](#-日本語)

---

## 🇪🇸 Español

**The Mechanic's Apprentice** es un juego de acción y supervivencia 3D con vista isométrica, desarrollado desde cero utilizando un motor propio construido en C++. Este proyecto demuestra habilidades avanzadas en arquitectura de software de bajo nivel, desarrollo de herramientas personalizadas (Tooling) y diseño de sistemas escalables (Data-Driven).

### 🎮 Jugar la Demo
* Jugar en Itch.io (PC Windows) - [Enlace a tu Itch.io]
* Ver gameplay y editor en YouTube - [Enlace a tu YouTube]

### 🛠️ Tecnologías y Herramientas
* **Motor Core:** C++17, Raylib, CMake
* **Editor de Niveles:** C# (.NET WinForms)
* **Patrones:** Arquitectura Data-Driven, State Machine, Factory, RAII.

### 🚀 Desafíos Técnicos Resueltos (Highlights)
* **Desarrollo de Herramientas y Pipeline Data-Driven:** Creación de un Editor de Niveles completo y desacoplado en C# (WinForms). El editor y el motor de C++ se comunican mediante esquemas JSON robustos, permitiendo editar estadísticas de enemigos, colocar estructuras (modulares y cilíndricas) y testear los mapas en tiempo real sin recompilar.
* **Sistema de Localización CJK Dinámico:** Implementación de un gestor de idiomas desde cero con soporte para Español, Inglés y Japonés. Incluye carga dinámica de glifos en memoria desde fuentes TTF, renderizado dual según el tamaño del texto para evitar artefactos (filtro pixel-perfect), y fallback de caracteres para interfaces seguras.
* **Sistemas de Combate, Físicas y Game Feel:** Motor de colisiones 3D propio para el combate, incluyendo daño en área (Cleave) multiobjetivo, cálculos de línea de visión (LoS) a través de obstáculos y gestión de estados de enemigos (Spawners, persecución, explosiones en cadena). Incorpora un gran "Game Feel" mediante *Hit-stop*, *Screen-shake* y shaders Toon con *Inverted Hull* para los contornos.

---

## 🇬🇧 English

**The Mechanic's Apprentice** is an isometric 3D action-survival game, developed from scratch using a custom-built engine in C++. This project showcases advanced skills in low-level software architecture, custom tool development (Tooling), and scalable systems design (Data-Driven architecture).

<<<<<<< HEAD
*(Nota: Para crear nuevos niveles, compila y ejecuta el proyecto `level-editor-csharp` de la carpeta raíz (o ábrelo desde el propio juego con el botón "Editor de Niveles" del menú principal / F12), diseña tu nivel y expórtalo a `engine-cpp/assets/data/level_<N>.json` o `engine-cpp/assets/data/endless.json` -- son los únicos archivos que el motor C++ carga.)*
=======
### 🎮 Play the Demo
* Play on Itch.io (PC Windows) - [Link to your Itch.io]
* Watch gameplay & editor on YouTube - [Link to your YouTube]

### 🛠️ Technologies & Tools
* **Core Engine:** C++17, Raylib, CMake
* **Level Editor:** C# (.NET WinForms)
* **Patterns:** Data-Driven Architecture, State Machine, Factory, RAII.

### 🚀 Technical Highlights
* **Custom Tooling & Data-Driven Pipeline:** Created a fully standalone Level Editor in C# (WinForms). The editor and the C++ engine communicate via robust JSON schemas, allowing real-time editing of enemy stats, modular/cylindrical structure placement, and live map testing without recompiling the game.
* **Dynamic CJK Localization System:** Built a custom localization manager supporting Spanish, English, and Japanese. Features dynamic glyph memory loading from TTF fonts, dual-size rendering to prevent visual artifacts (pixel-perfect filtering), and safe character fallback for UI elements.
* **Combat Systems, Physics & Game Feel:** Custom 3D collision engine featuring multi-target area-of-effect (Cleave) damage, Line of Sight (LoS) checks through obstacles, and robust enemy state management (Spawners, pathing, chain explosions). Enhanced with strong "Game Feel" techniques including Hit-stop, Screen-shake, and Toon shaders with Inverted Hull outlines.

---

## 🇯🇵 日本語

**The Mechanic's Apprentice（メカニックの見習い）**は、C++で構築されたカスタムエンジンを使用してゼロから開発された、アイソメトリック3Dアクションサバイバルゲームです。このプロジェクトは、低レベルのソフトウェアアーキテクチャ、独自ツールの開発、およびデータドリブン設計における高度なスキルを示しています。

### 🎮 デモをプレイする
* Itch.ioでプレイ (PC Windows) - [Itch.ioのリンク]
* YouTubeでゲームプレイとエディタを見る - [YouTubeのリンク]

### 🛠️ 使用技術とツール
* **コアエンジン:** C++17, Raylib, CMake
* **レベルエディタ:** C# (.NET WinForms)
* **設計パターン:** データドリブン・アーキテクチャ、ステートマシン、Factory、RAII

### 🚀 主な技術的ハイライト
* **独自ツールとデータドリブン・パイプライン:** C# (WinForms) で完全に独立したレベルエディタを作成。エディタとC++エンジンは堅牢なJSONスキーマを介して通信し、再コンパイルなしで敵のステータス編集、モジュール構造の配置、マップのテストをリアルタイムで可能にしました。
* **動的CJKローカライゼーションシステム:** スペイン語、英語、日本語をサポートする独自の言語管理システムを実装。TTFフォントからの動的グリフロード、アーティファクトを防ぐためのデュアルサイズレンダリング（ピクセルパーフェクト）、および安全なUIのための文字フォールバック機能を備えています。
* **戦闘システム・物理演算・ゲームフィール:** 複数ターゲットへの範囲攻撃（Cleave）、障害物を越える視線（LoS）判定、敵の高度なステート管理（スポナー、追跡、連鎖爆発）を備えた独自の3D衝突判定エンジン。ヒットストップ、画面揺れ（スクリーンシェイク）、背面法線反転（Inverted Hull）によるトゥーンシェーダーなど、優れた「ゲームフィール」を実現しています。
>>>>>>> 160cbaaefa3807941d2602d3e30325d15be75641
