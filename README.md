# ⚙️ The Mechanic's Apprentice

[Español](#-español) | [English](#-english) | [日本語](#-日本語)

---

## 🇪🇸 Español

**The Mechanic's Apprentice** es un juego de acción y supervivencia 3D con vista isométrica, desarrollado desde cero utilizando un motor propio construido en C++. Este proyecto demuestra habilidades en arquitectura de software de bajo nivel, desarrollo de herramientas personalizadas (Tooling) y diseño de sistemas escalables (Data-Driven).

### 🎮 Jugar la Demo
* Jugar en Itch.io (PC Windows) - <https://sergiog-7.itch.io/the-mechanics-apprentice> (contraseña: level5)
* Ver gameplay en YouTube - <https://youtu.be/KxtVA9pftCg>
* Ver editor en YouTube - <https://youtu.be/jk4LX89isQ4>

### 🛠️ Tecnologías y Herramientas
* **Motor Core:** C++17, Raylib, CMake
* **Editor de Niveles:** C# (.NET WinForms)
* **Patrones:** Arquitectura Data-Driven, State Machine, Factory, RAII.

### 🚀 Desafíos Técnicos
* **Desarrollo de Herramientas y Pipeline Data-Driven:** Creación de un Editor de Niveles desacoplado en C# (WinForms). El editor y el motor de C++ se comunican mediante esquemas JSON robustos, permitiendo editar estadísticas de enemigos, colocar estructuras (modulares y cilíndricas) y testear los mapas en tiempo real sin necesidad de recompilar.
* **Sistema de Localización CJK Dinámico:** Implementación de un gestor de idiomas propio con soporte para Español, Inglés y Japonés. Incluye carga dinámica de glifos en memoria desde fuentes TTF, generación de atlas 1:1 para cada tamaño de texto exacto (renderizado pixel-perfect sin artefactos de escalado) y sistema de fallback para evitar fallos de renderizado en la interfaz.
* **Sistemas de Combate, Físicas y Game Feel:** Motor de colisiones 3D propio para el combate, incluyendo daño en área (Cleave) multiobjetivo, cálculos de oclusión de línea de visión (LoS) mediante obstáculos y máquinas de estados para enemigos (Spawners, persecución, explosiones en cadena). Incorpora un gran "Game Feel" mediante *Hit-stop*, *Screen-shake* y shaders Toon con contornos mediante *Inverted Hull*.

---

## 🇬🇧 English

**The Mechanic's Apprentice** is an isometric 3D action-survival game, developed from scratch using a custom-built engine in C++. This project demonstrates solid foundations in low-level software architecture, custom tool development (Tooling), and scalable systems design (Data-Driven architecture).

### 🎮 Play the Demo
* Play on Itch.io (PC Windows) - <https://sergiog-7.itch.io/the-mechanics-apprentice> (password: level5)
* Watch gameplay on YouTube - <https://youtu.be/KxtVA9pftCg>
* Watch editor on YouTube - <https://youtu.be/jk4LX89isQ4>

### 🛠️ Technologies & Tools
* **Core Engine:** C++17, Raylib, CMake
* **Level Editor:** C# (.NET WinForms)
* **Patterns:** Data-Driven Architecture, State Machine, Factory, RAII.

### 🚀 Technical Challenges
* **Custom Tooling & Data-Driven Pipeline:** Developed a standalone Level Editor in C# (WinForms). The editor and the C++ engine communicate via robust JSON schemas, enabling live enemy stat tweaking, modular/cylindrical structure placement, and real-time map testing without recompiling the game.
* **Dynamic CJK Localization System:** Built a custom localization manager supporting Spanish, English, and Japanese. Features dynamic glyph memory allocation from TTF fonts, exact 1:1 atlas baking per font size for pixel-perfect rendering with zero scaling artifacts, and font fallbacks to prevent missing glyphs across the UI.
* **Combat Systems, Physics & Game Feel:** Custom 3D collision engine featuring multi-target area-of-effect (Cleave) damage, Line of Sight (LoS) obstacle occlusion checks, and robust enemy state management (spawners, chase behavior, chain reactions). Enhanced game feel via Hit-stop, Screen-shake, and Toon shaders with Inverted Hull outlines.

---

## 🇯🇵 日本語

**The Mechanic's Apprentice（メカニックの見習い）**は、C++で構築された自作エンジンを用いてゼロから開発した、アイソメトリック3Dアクションサバイバルゲームです。低レベルのソフトウェア設計、専用ツールの開発、およびデータ駆動型（Data-Driven）アーキテクチャの実装力を示しています。

### 🎮 デモをプレイする
* Itch.ioでプレイ (PC Windows) - <https://sergiog-7.itch.io/the-mechanics-apprentice> (パスワード：level5)
* YouTubeでゲームプレイを見る - <https://youtu.be/KxtVA9pftCg>
* YouTubeでエディタを見る - <https://youtu.be/jk4LX89isQ4>

### 🛠️ 使用技術とツール
* **コアエンジン:** C++17, Raylib, CMake
* **レベルエディタ:** C# (.NET WinForms)
* **設計パターン:** データドリブン・アーキテクチャ、ステートマシン、Factory、RAII

### 🚀 技術的課題
* **専用ツール開発とデータ駆動型パイプライン:** C# (WinForms) で独立したレベルエディタを自作。エディタとC++エンジン間は堅牢なJSONスキーマで通信し、ゲームを再コンパイルすることなく、敵パラメータの調整、構造物（モジュール・円柱）の配置、マップのリアルタイムテストを可能にしました。
* **動的CJKローカライゼーションシステム:** スペイン語、英語、日本語に対応する独自の言語管理システムを実装。TTFフォントからの動的グリフ展開、文字サイズごとに1:1でアトラスを生成する劣化のないピクセルパーフェクト描画、および文字化け（トーフ）や表示崩れを防ぐフォントフォールバック機能を備えています。
* **戦闘システム・物理演算・ゲームフィール:** 複数ターゲットへの範囲攻撃（Cleave）、障害物による射線・視線遮蔽（LoS）判定、敵のステート管理（スポナー、追従AI、連鎖爆発）を備えた自作の3D衝突判定エンジンを構築。ヒットストップ、画面揺れ（スクリーンシェイク）、背面法線反転（Inverted Hull）によるトゥーン輪郭描画など、手触りの良い「ゲームフィール」を実現しました。
