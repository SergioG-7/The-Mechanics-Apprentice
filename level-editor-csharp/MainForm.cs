using System.Drawing.Drawing2D;
using LevelEditor.Core;
using LevelEditor.Localization;
using LevelEditor.Models;
using static LevelEditor.Canvas.CanvasGeometry;

namespace LevelEditor
{
    // Sin diseñador visual (a propósito, por decisión del proyecto): toda la
    // UI, el dibujado del grid y el panel de propiedades se arman por código.
    public class MainForm : Form
    {
        private enum EditorTool
        {
            PlacePlayer,
            PlaceEnemy,
            PlaceObstacle,
            PlaceCylinder,
            PlaceHazard,
            PlaceElectricTile,
            PlaceGear,
            PlaceHealthKit,
            PlaceBarrel,
            PlacePowerUp,
            PlaceDoor,
            PlaceSpawner,
            DefinePatrol,
            Select
        }

        // Arquetipos disponibles para el ComboBox de variante y para el tipo
        // de enemigo de un Spawner. "Default" es el único que NO pasa por
        // EnemyFactory en el motor C++: usa los stats propios del EnemyData.
        // Son identificadores consumidos tal cual por el motor (no se
        // traducen NUNCA como valor) -- lo que se traduce es solo su
        // representación en el ComboBox, ver BuildVariantItems.
        private static readonly string[] EnemyVariantNames =
            { "Default", "Tank", "Runner", "Spitter", "Kamikaze", "Shielder", "Buffer", "Trapper" };

        // Tipos de power-up que reconoce PowerUp::ParseType en el motor C++.
        // Identificadores, no texto de interfaz -- ver PowerUpData.Type.
        private static readonly string[] PowerUpTypeNames = { "Overclock", "Frenzy", "Shield" };

        // Mismo envoltorio que BuildVariantItems pero con claves
        // "powerup_{code}" (powerup_Overclock, powerup_Frenzy...).
        private static ComboBoxItem<string>[] BuildPowerUpItems() =>
            PowerUpTypeNames.Select(code => new ComboBoxItem<string>(code, LocalizationManager.GetText($"powerup_{code}"))).ToArray();

        // Envuelve cada código de variante en un ComboBoxItem cuyo texto
        // visible sale de "variant_{code}" (variant_Default, variant_Tank...)
        // en el idioma activo, pero cuyo Value se queda en el código en
        // inglés que espera EnemyFactory -- así el ComboBox se relocaliza
        // sin arrastrar el valor serializado.
        private static ComboBoxItem<string>[] BuildVariantItems(IEnumerable<string> codes) =>
            codes.Select(code => new ComboBoxItem<string>(code, LocalizationManager.GetText($"variant_{code}"))).ToArray();

        // --- Estado del nivel en memoria ---
        private PlayerData? _player;
        private readonly List<EnemyData> _enemies = new();
        private readonly List<ObstacleData> _obstacles = new();
        private readonly List<GearData> _gears = new();
        private readonly List<SpawnerData> _spawners = new();
        private readonly List<HealthKitData> _healthKits = new();
        private readonly List<BarrelData> _barrels = new();
        private readonly List<HazardData> _hazards = new();
        private readonly List<PowerUpData> _powerUps = new();
        private readonly List<ElectricTileData> _electricTiles = new();
        private DoorData? _door; // singular, como el Player: colocarla de nuevo reemplaza la anterior

        private EditorTool _activeTool = EditorTool.PlacePlayer;
        private object? _selectedEntity;

        // Todos los RadioButton de herramienta, repartidos entre el
        // TabControl y el GroupBox de Edición (dos contenedores distintos,
        // así que WinForms no los agrupa solo por sí mismo): SelectTool los
        // recorre para que solo uno quede marcado a la vez sin importar en
        // qué pestaña o grupo esté. La clave de idioma va emparejada para
        // poder reetiquetarlos todos al cambiar de idioma (ver ApplyLanguage).
        private readonly List<(RadioButton Radio, string TextKey)> _toolRadios = new();

        // Botón de idioma por código ("es"/"en"/"jp"): su Text nunca cambia
        // (cada uno se rotula en su propio idioma, ver CreateLanguageButton),
        // pero ApplyLanguage necesita encontrarlos para resaltar el activo.
        private readonly Dictionary<string, Button> _languageButtons = new();

        // Variante activa en el ComboBox de "Variante de enemigo": se aplica
        // tanto a un EnemyData nuevo (PlaceEnemy) como al EnemyType de un
        // SpawnerData nuevo (PlaceSpawner) -- un solo selector para ambos.
        private string _selectedVariant = "Default";

        // Tipo activo en el ComboBox de "Tipo de power-up", aplicado al
        // colocar uno nuevo (PlacePowerUp) -- mismo papel que _selectedVariant.
        private string _selectedPowerUpType = "Overclock";

        // Nombre lógico del nivel (campo "levelName" del JSON). Se conserva
        // al abrir un nivel existente para no perderlo al reexportar.
        private string _levelName = "arena_editor";

        // Ruta del último archivo abierto/exportado (null = nivel nuevo, nunca guardado)
        // y si hay cambios desde esa operación. Se reflejan en el título de la ventana.
        private string? _currentFilePath;
        private bool _isDirty;

        // Entidad que se está arrastrando con el botón izquierdo (herramienta Seleccionar), o null.
        private object? _draggingEntity;

        // --- Controles ---
        private readonly Panel _canvasPanel;
        private readonly GroupBox _propertiesGroup;
        private readonly Label _statusLabel;

        // Controles cuyo texto depende del idioma activo y que ApplyLanguage
        // necesita reetiquetar en caliente al cambiar de idioma.
        private readonly TabPage _entitiesTab;
        private readonly TabPage _objectsTab;
        private readonly TabPage _systemTab;
        private readonly GroupBox _editGroup;
        private readonly GroupBox _variantGroup;
        private readonly ComboBox _variantCombo;
        private readonly GroupBox _powerUpGroup;
        private readonly ComboBox _powerUpCombo;
        private readonly GroupBox _languageGroup;
        private readonly Label _hintLabel;
        private readonly Button _openButton;
        private readonly Button _exportButton;

        public MainForm()
        {
            // Alto = el del lienzo ampliado (700) más los márgenes: al pasar
            // de 600 a 700 px de canvas, dejarlo en 700 recortaba la última
            // fila de celdas del propio grid.
            ClientSize = new Size(CanvasSize + 240, CanvasSize + 60);
            StartPosition = FormStartPosition.CenterScreen;
            // El panel derecho ya se salía de los 700px de alto antes de esta
            // fase (propertiesGroup solo mide 300px de sus 260 originales) y
            // la reorganización en pestañas lo alarga un poco más -- en vez
            // de perseguir un número de píxeles exacto cada vez que se añade
            // una herramienta, el formulario hace scroll si hace falta.
            AutoScroll = true;

            // --- Lienzo ---
            _canvasPanel = new Panel
            {
                Location = new Point(20, 20),
                Size = new Size(CanvasSize, CanvasSize),
                BackColor = Color.White,
                BorderStyle = BorderStyle.FixedSingle
            };
            _canvasPanel.Paint += OnCanvasPaint;
            _canvasPanel.MouseClick += OnCanvasMouseClick;
            _canvasPanel.MouseDown += OnCanvasMouseDown;
            _canvasPanel.MouseMove += OnCanvasMouseMove;
            _canvasPanel.MouseUp += OnCanvasMouseUp;
            Controls.Add(_canvasPanel);

            int toolsX = _canvasPanel.Right + 20;

            // --- Idioma: agrupado y siempre visible arriba del todo, fuera
            // de las pestañas -- es un ajuste de la aplicación, no una
            // herramienta de edición de nivel. Cada botón se rotula en su
            // propio idioma y con su propia fuente (ver CreateLanguageButton),
            // así que se lee bien pase lo que pase con el idioma activo.
            _languageGroup = new GroupBox
            {
                Text = LocalizationManager.GetText("group_language"),
                Location = new Point(toolsX, 20),
                Size = new Size(190, 100)
            };
            _languageGroup.Controls.Add(CreateLanguageButton("es", "Español", 20));
            _languageGroup.Controls.Add(CreateLanguageButton("en", "English", 45));
            _languageGroup.Controls.Add(CreateLanguageButton("jp", "日本語", 70));
            Controls.Add(_languageGroup);

            // --- Herramientas: categorizadas en pestañas para que no crezcan
            // indefinidamente en vertical (Fase 5). Un RadioButton por pestaña
            // se agrupa solo con los de su MISMA pestaña por defecto en
            // WinForms -- SelectTool (más abajo) fuerza la exclusión mutua a
            // mano entre TODAS las herramientas, tabs y grupo de Edición
            // incluidos, para que solo una quede activa a la vez.
            var toolTabs = new TabControl
            {
                Location = new Point(toolsX, _languageGroup.Bottom + 10),
                Size = new Size(190, 250) // 250: la pestaña Entidades llegó a 7 herramientas (la última en y=190)
            };

            _entitiesTab = new TabPage(LocalizationManager.GetText("tab_entities"));
            CreateToolRadio("tool_place_player", 10, EditorTool.PlacePlayer, _entitiesTab, startChecked: true);
            CreateToolRadio("tool_place_enemy", 40, EditorTool.PlaceEnemy, _entitiesTab);
            CreateToolRadio("tool_place_obstacle", 70, EditorTool.PlaceObstacle, _entitiesTab);
            CreateToolRadio("tool_place_cylinder", 100, EditorTool.PlaceCylinder, _entitiesTab);
            CreateToolRadio("tool_place_hazard", 130, EditorTool.PlaceHazard, _entitiesTab);
            CreateToolRadio("tool_place_electrictile", 160, EditorTool.PlaceElectricTile, _entitiesTab);
            CreateToolRadio("tool_place_door", 190, EditorTool.PlaceDoor, _entitiesTab);
            toolTabs.TabPages.Add(_entitiesTab);

            _objectsTab = new TabPage(LocalizationManager.GetText("tab_objects"));
            CreateToolRadio("tool_place_gear", 10, EditorTool.PlaceGear, _objectsTab);
            CreateToolRadio("tool_place_healthkit", 40, EditorTool.PlaceHealthKit, _objectsTab);
            CreateToolRadio("tool_place_barrel", 70, EditorTool.PlaceBarrel, _objectsTab);
            CreateToolRadio("tool_place_powerup", 100, EditorTool.PlacePowerUp, _objectsTab);
            toolTabs.TabPages.Add(_objectsTab);

            _systemTab = new TabPage(LocalizationManager.GetText("tab_system"));
            CreateToolRadio("tool_place_spawner", 10, EditorTool.PlaceSpawner, _systemTab);
            toolTabs.TabPages.Add(_systemTab);

            Controls.Add(toolTabs);

            // --- Edición: modales que actúan sobre la selección, no
            // "colocan" nada -- fuera de las pestañas a propósito, siempre
            // visibles sea cual sea la categoría activa. ---
            _editGroup = new GroupBox
            {
                Text = LocalizationManager.GetText("group_edit"),
                Location = new Point(toolsX, toolTabs.Bottom + 10),
                Size = new Size(190, 90)
            };
            CreateToolRadio("tool_define_patrol", 25, EditorTool.DefinePatrol, _editGroup);
            CreateToolRadio("tool_select", 55, EditorTool.Select, _editGroup);
            Controls.Add(_editGroup);

            // --- Variante de enemigo (se aplica al colocar Enemigo o Spawner) ---
            _variantGroup = new GroupBox
            {
                Text = LocalizationManager.GetText("group_variant"),
                Location = new Point(toolsX, _editGroup.Bottom + 10),
                Size = new Size(190, 55)
            };
            _variantCombo = new ComboBox
            {
                Location = new Point(10, 20),
                Width = 160,
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            // El VALOR (EnemyVariantNames) es el identificador que consume
            // EnemyFactory en el motor C++ (ver LevelLoader.cpp) y nunca se
            // traduce; el TEXTO visible sí, vía ComboBoxItem (ver
            // BuildVariantItems) -- RefreshVariantCombo lo reetiqueta al
            // cambiar de idioma sin tocar _selectedVariant.
            _variantCombo.Items.AddRange(BuildVariantItems(EnemyVariantNames));
            _variantCombo.SelectedIndex = 0;
            _variantCombo.SelectedIndexChanged += OnVariantComboChanged;
            _variantGroup.Controls.Add(_variantCombo);
            Controls.Add(_variantGroup);

            // --- Tipo de power-up (se aplica al colocar un Power-Up) ---
            _powerUpGroup = new GroupBox
            {
                Text = LocalizationManager.GetText("group_powerup"),
                Location = new Point(toolsX, _variantGroup.Bottom + 10),
                Size = new Size(190, 55)
            };
            _powerUpCombo = new ComboBox
            {
                Location = new Point(10, 20),
                Width = 160,
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            _powerUpCombo.Items.AddRange(BuildPowerUpItems());
            _powerUpCombo.SelectedIndex = 0;
            _powerUpCombo.SelectedIndexChanged += OnPowerUpComboChanged;
            _powerUpGroup.Controls.Add(_powerUpCombo);
            Controls.Add(_powerUpGroup);

            _hintLabel = new Label
            {
                Text = LocalizationManager.GetText("hint_delete"),
                Location = new Point(toolsX, _powerUpGroup.Bottom + 5),
                Size = new Size(190, 30),
                ForeColor = Color.DimGray
            };
            Controls.Add(_hintLabel);

            // --- Abrir / Exportar ---
            _openButton = new Button
            {
                Text = LocalizationManager.GetText("btn_open"),
                Location = new Point(toolsX, _hintLabel.Bottom + 10),
                AutoSize = true
            };
            _openButton.Click += OnOpenButtonClick;
            Controls.Add(_openButton);

            _exportButton = new Button
            {
                Text = LocalizationManager.GetText("btn_export"),
                Location = new Point(toolsX, _openButton.Bottom + 10),
                AutoSize = true
            };
            _exportButton.Click += OnExportButtonClick;
            Controls.Add(_exportButton);

            // --- Propiedades (contenido dinámico, ver RefreshPropertiesPanel) ---
            _propertiesGroup = new GroupBox
            {
                Text = LocalizationManager.GetText("group_properties"),
                Location = new Point(toolsX, _exportButton.Bottom + 20),
                Size = new Size(190, 300), // 300, no 260: BuildEnemyProperties tiene 5 filas desde que se añadió el ComboBox de Type
                Visible = false
            };
            Controls.Add(_propertiesGroup);

            // --- Estado ---
            _statusLabel = new Label
            {
                Location = new Point(toolsX, _propertiesGroup.Bottom + 20),
                Size = new Size(190, 185) // 185: BuildStatusText tiene 11 líneas desde que se añadieron las baldosas
            };
            Controls.Add(_statusLabel);

            ApplyLanguage();
        }

        // Crea un RadioButton de herramienta, lo añade al contenedor dado
        // (una TabPage o el GroupBox de Edición) y lo registra en
        // _toolRadios para que SelectTool pueda desmarcar los demás sea cual
        // sea su contenedor, y para que ApplyLanguage pueda reetiquetarlo.
        private RadioButton CreateToolRadio(string textKey, int y, EditorTool tool, Control container, bool startChecked = false)
        {
            var radio = new RadioButton
            {
                Location = new Point(10, y),
                AutoSize = true,
                Checked = startChecked
            };
            radio.Click += (s, e) => SelectTool(radio, tool);
            container.Controls.Add(radio);
            _toolRadios.Add((radio, textKey));
            ApplyLocalizedText(radio, textKey);
            return radio;
        }

        // Única fuente de verdad de qué herramienta está activa. Los radios
        // están repartidos en varios contenedores (TabPages + GroupBox de
        // Edición), así que WinForms NO los agrupa por sí solo -- de ahí que
        // cada radio dispare esto en Click (no CheckedChanged) y aquí se
        // desmarquen a mano todos los demás.
        private void SelectTool(RadioButton chosen, EditorTool tool)
        {
            foreach (var (radio, _) in _toolRadios)
            {
                radio.Checked = ReferenceEquals(radio, chosen);
            }
            _activeTool = tool;
        }

        // --- Localización ---

        // Crea (o encuentra) el botón de un idioma. El texto es SIEMPRE el
        // nombre nativo del idioma -- no pasa por LocalizationManager, se lee
        // igual sea cual sea el idioma activo (como el selector de idioma de
        // cualquier aplicación real, y como NativeLanguageName en el motor
        // C++). La fuente sí se resuelve por contenido, para que 日本語 se
        // lea bien aunque el idioma activo sea español.
        private Button CreateLanguageButton(string code, string nativeLabel, int y)
        {
            var button = new Button
            {
                Text = nativeLabel,
                Location = new Point(10, y),
                Size = new Size(170, 22),
                TextAlign = ContentAlignment.MiddleCenter
            };
            if (LocalizationManager.ContainsNonAscii(nativeLabel))
            {
                button.Font = FontResolver.ResolveCjkFont(Font.Size);
            }
            button.Click += (s, e) => SetLanguage(code);
            _languageButtons[code] = button;
            return button;
        }

        private void SetLanguage(string code)
        {
            LocalizationManager.SetLanguage(code);
            EditorSettings.Save(new EditorSettingsData { Language = code });
            ApplyLanguage();
        }

        private void OnVariantComboChanged(object? sender, EventArgs e)
        {
            _selectedVariant = ((ComboBoxItem<string>)_variantCombo.SelectedItem!).Value;
        }

        private void OnPowerUpComboChanged(object? sender, EventArgs e)
        {
            _selectedPowerUpType = ((ComboBoxItem<string>)_powerUpCombo.SelectedItem!).Value;
        }

        // Reetiqueta el ComboBox de variante superior (fuera del panel de
        // propiedades, así que ApplyLanguage no lo reconstruye gratis como
        // hace con BuildEnemyProperties/BuildSpawnerProperties vía
        // RefreshPropertiesPanel). Se desengancha el handler mientras
        // reconstruye los Items: un Items.Clear() dispara SelectedIndexChanged
        // con SelectedItem a null, y el cast de OnVariantComboChanged
        // reventaría con ese estado transitorio.
        private void RefreshVariantCombo()
        {
            _variantCombo.SelectedIndexChanged -= OnVariantComboChanged;
            _variantCombo.Items.Clear();
            _variantCombo.Items.AddRange(BuildVariantItems(EnemyVariantNames));
            int index = Array.IndexOf(EnemyVariantNames, _selectedVariant);
            _variantCombo.SelectedIndex = index >= 0 ? index : 0;
            _variantCombo.SelectedIndexChanged += OnVariantComboChanged;
        }

        // Igual que RefreshVariantCombo, con el mismo desenganche del handler
        // mientras se reconstruyen los Items (ver el comentario de arriba).
        private void RefreshPowerUpCombo()
        {
            _powerUpCombo.SelectedIndexChanged -= OnPowerUpComboChanged;
            _powerUpCombo.Items.Clear();
            _powerUpCombo.Items.AddRange(BuildPowerUpItems());
            int index = Array.IndexOf(PowerUpTypeNames, _selectedPowerUpType);
            _powerUpCombo.SelectedIndex = index >= 0 ? index : 0;
            _powerUpCombo.SelectedIndexChanged += OnPowerUpComboChanged;
        }

        // Escribe el texto correcto en cada control fijo del formulario para
        // el idioma activo, resolviendo la fuente por CONTENIDO (no por
        // idioma activo -- ver Meta/patrones/localizacion-cjk-unity.md):
        // una etiqueta que sigue siendo ASCII en el idioma nuevo recupera la
        // fuente por defecto explícitamente, no se queda con la CJK puesta.
        private void ApplyLocalizedText(Control control, string textKey)
        {
            string text = LocalizationManager.GetText(textKey);
            control.Text = text;
            control.Font = LocalizationManager.ContainsNonAscii(text)
                ? FontResolver.ResolveCjkFont(Font.Size, control.Font.Style)
                : Font;
        }

        // Usado por los constructores de panel de propiedades (BuildPlayerProperties
        // y hermanos), que crean sus Label directamente en vez de sobre un
        // control ya existente.
        private Label CreateLocalizedLabel(string textKey, Point location)
        {
            string text = LocalizationManager.GetText(textKey);
            var label = new Label { Location = location, AutoSize = true, Text = text };
            if (LocalizationManager.ContainsNonAscii(text))
            {
                label.Font = FontResolver.ResolveCjkFont(Font.Size);
            }
            return label;
        }

        // Punto único que reetiqueta TODO lo que ya está en pantalla al
        // cambiar de idioma (o al construir el formulario la primera vez).
        // El panel de propiedades se reconstruye entero porque sus Label se
        // crean sobre la marcha en BuildXxxProperties -- más simple que
        // llevar una lista aparte de sus controles.
        private void ApplyLanguage()
        {
            ApplyLocalizedText(_entitiesTab, "tab_entities");
            ApplyLocalizedText(_objectsTab, "tab_objects");
            ApplyLocalizedText(_systemTab, "tab_system");

            foreach (var (radio, textKey) in _toolRadios)
            {
                ApplyLocalizedText(radio, textKey);
            }

            ApplyLocalizedText(_editGroup, "group_edit");
            ApplyLocalizedText(_variantGroup, "group_variant");
            RefreshVariantCombo();
            ApplyLocalizedText(_powerUpGroup, "group_powerup");
            RefreshPowerUpCombo();
            ApplyLocalizedText(_languageGroup, "group_language");
            ApplyLocalizedText(_hintLabel, "hint_delete");
            ApplyLocalizedText(_openButton, "btn_open");
            ApplyLocalizedText(_exportButton, "btn_export");
            ApplyLocalizedText(_propertiesGroup, "group_properties");

            foreach (var (code, button) in _languageButtons)
            {
                FontStyle style = code == LocalizationManager.CurrentLanguage ? FontStyle.Bold : FontStyle.Regular;
                button.Font = LocalizationManager.ContainsNonAscii(button.Text)
                    ? FontResolver.ResolveCjkFont(Font.Size, style)
                    : new Font(Font, style);
            }

            RefreshPropertiesPanel();
            RefreshStatusLabel();
            UpdateTitle();
        }

        // --- Título / estado de guardado ---

        private void UpdateTitle()
        {
            string fileLabel = _currentFilePath is null
                ? LocalizationManager.GetText("title_unsaved")
                : Path.GetFileName(_currentFilePath);
            Text = $"{LocalizationManager.GetText("title_app")} — {fileLabel}{(_isDirty ? " *" : "")}";
        }

        private void MarkDirty()
        {
            _isDirty = true;
            UpdateTitle();
        }

        // --- Clic en el lienzo ---

        private void OnCanvasMouseClick(object? sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                HandleRightClick(e.Location);
                RefreshStatusLabel();
                _canvasPanel.Invalidate();
                return;
            }

            Vector3Data worldPos = ScreenToWorld(e.Location);

            switch (_activeTool)
            {
                case EditorTool.PlacePlayer:
                    _player = new PlayerData { Spawn = worldPos, MaxHP = 100.0f, Speed = 4.0f, AttackDamage = 50.0f };
                    MarkDirty();
                    break;

                case EditorTool.PlaceEnemy:
                    // Stats base del arquetipo elegido (ver EnemyVariantCatalog) si
                    // existe una en enemy_variants.json; si no ("Default", o una
                    // variante que el catálogo no reconoce), los valores de
                    // siempre -- mismo criterio de degradar sin reventar.
                    EnemyVariantStatsData? placedVariantStats = EnemyVariantCatalog.TryGet(_selectedVariant);
                    _enemies.Add(new EnemyData
                    {
                        Spawn = worldPos,
                        Type = _selectedVariant,
                        MaxHP = placedVariantStats?.MaxHP ?? 30.0f,
                        VisionRadius = placedVariantStats?.VisionRadius ?? 6.0f,
                        Speed = placedVariantStats?.Speed ?? 2.5f,
                        AttackDamage = placedVariantStats?.AttackDamage ?? 10.0f,
                        PatrolRoute = new List<Vector3Data>()
                    });
                    MarkDirty();
                    break;

                case EditorTool.PlaceObstacle:
                    _obstacles.Add(new ObstacleData
                    {
                        Position = worldPos,
                        Type = "box",
                        Size = new Vector3Data(1.0f, 1.0f, 1.0f)
                    });
                    MarkDirty();
                    break;

                case EditorTool.PlaceCylinder:
                    _obstacles.Add(new ObstacleData
                    {
                        Position = worldPos,
                        Type = "cylinder",
                        Radius = 0.6f,
                        Height = 2.5f
                    });
                    MarkDirty();
                    break;

                case EditorTool.PlaceHazard:
                    _hazards.Add(new HazardData
                    {
                        Position = worldPos,
                        Size = new Vector3Data(2.0f, 0.1f, 2.0f),
                        DamagePerTick = 10.0f
                    });
                    MarkDirty();
                    break;

                case EditorTool.PlaceElectricTile:
                    // CycleInterval 0 por defecto: se arma solo al pisarla,
                    // que es el comportamiento más fácil de leer. Ponerle un
                    // ciclo es una decisión consciente desde Propiedades.
                    _electricTiles.Add(new ElectricTileData
                    {
                        Position = worldPos,
                        Size = new Vector3Data(2.0f, 0.1f, 2.0f),
                        Damage = 20.0f,
                        CycleInterval = 0.0f
                    });
                    MarkDirty();
                    break;

                case EditorTool.PlaceGear:
                    _gears.Add(new GearData { Position = worldPos });
                    MarkDirty();
                    break;

                case EditorTool.PlaceHealthKit:
                    _healthKits.Add(new HealthKitData { Position = worldPos });
                    MarkDirty();
                    break;

                case EditorTool.PlaceBarrel:
                    _barrels.Add(new BarrelData { Position = worldPos });
                    MarkDirty();
                    break;

                case EditorTool.PlacePowerUp:
                    _powerUps.Add(new PowerUpData { Position = worldPos, Type = _selectedPowerUpType });
                    MarkDirty();
                    break;

                case EditorTool.PlaceDoor:
                    _door = new DoorData { Position = worldPos, HalfExtents = new Vector3Data(1.0f, 1.0f, 1.0f) };
                    MarkDirty();
                    break;

                case EditorTool.PlaceSpawner:
                    _spawners.Add(new SpawnerData
                    {
                        Position = worldPos,
                        // "Default" no es una variante real de EnemyFactory (solo
                        // tiene sentido para un EnemyData suelto, con sus propios
                        // stats) -- un Spawner con ese tipo no generaría nunca
                        // nada. Cae a "Runner" en ese caso.
                        EnemyType = _selectedVariant == "Default" ? "Runner" : _selectedVariant,
                        Interval = 4.0f,
                        MaxEnemies = 3
                    });
                    MarkDirty();
                    break;

                case EditorTool.DefinePatrol:
                    // Comprobamos si el objeto seleccionado actualmente es un enemigo
                    if (_selectedEntity is EnemyData selectedEnemy)
                    {
                        selectedEnemy.PatrolRoute.Add(worldPos);
                        MarkDirty();
                    }
                    else
                    {
                        MessageBox.Show(LocalizationManager.GetText("msg_no_enemy_body"),
                                        LocalizationManager.GetText("msg_no_enemy_title"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    }
                    break;

                case EditorTool.Select:
                    _selectedEntity = FindEntityAt(e.Location);
                    RefreshPropertiesPanel();
                    break;
            }

            RefreshStatusLabel();
            _canvasPanel.Invalidate();
        }

        // Orden inverso al de dibujado (lo último dibujado, arriba del todo,
        // se prueba primero): Player, Enemies, Spawners, Barrels, PowerUps,
        // HealthKits, Gears, Hazards, Door, Obstacles (Obstacle-box y
        // Cylinder mezclados).
        private object? FindEntityAt(Point screenPoint)
        {
            if (_player is not null && IsPointNearMarker(screenPoint, WorldToScreen(_player.Spawn)))
                return _player;

            for (int i = _enemies.Count - 1; i >= 0; i--)
                if (IsPointNearMarker(screenPoint, WorldToScreen(_enemies[i].Spawn)))
                    return _enemies[i];

            for (int i = _spawners.Count - 1; i >= 0; i--)
                if (IsPointNearMarker(screenPoint, WorldToScreen(_spawners[i].Position)))
                    return _spawners[i];

            for (int i = _barrels.Count - 1; i >= 0; i--)
                if (IsPointNearMarker(screenPoint, WorldToScreen(_barrels[i].Position)))
                    return _barrels[i];

            for (int i = _powerUps.Count - 1; i >= 0; i--)
                if (IsPointNearMarker(screenPoint, WorldToScreen(_powerUps[i].Position)))
                    return _powerUps[i];

            for (int i = _healthKits.Count - 1; i >= 0; i--)
                if (IsPointNearMarker(screenPoint, WorldToScreen(_healthKits[i].Position)))
                    return _healthKits[i];

            for (int i = _gears.Count - 1; i >= 0; i--)
                if (IsPointNearMarker(screenPoint, WorldToScreen(_gears[i].Position)))
                    return _gears[i];

            for (int i = _hazards.Count - 1; i >= 0; i--)
                if (GetBoxScreenRect(_hazards[i].Position, GetHazardHalfExtents(_hazards[i])).Contains(screenPoint))
                    return _hazards[i];

            for (int i = _electricTiles.Count - 1; i >= 0; i--)
                if (GetBoxScreenRect(_electricTiles[i].Position, GetElectricTileHalfExtents(_electricTiles[i])).Contains(screenPoint))
                    return _electricTiles[i];

            if (_door is not null && GetBoxScreenRect(_door.Position, _door.HalfExtents).Contains(screenPoint))
                return _door;

            for (int i = _obstacles.Count - 1; i >= 0; i--)
                if (IsObstacleHit(_obstacles[i], screenPoint))
                    return _obstacles[i];

            return null;
        }

        // --- Arrastrar para mover (herramienta Seleccionar, botón izquierdo) ---

        private void OnCanvasMouseDown(object? sender, MouseEventArgs e)
        {
            if (_activeTool != EditorTool.Select || e.Button != MouseButtons.Left) return;

            object? entity = FindEntityAt(e.Location);
            if (entity is null) return;

            _draggingEntity = entity;
            _selectedEntity = entity;
            RefreshPropertiesPanel();
            _canvasPanel.Invalidate();
        }

        private void OnCanvasMouseMove(object? sender, MouseEventArgs e)
        {
            if (_draggingEntity is null) return;

            SetEntityPosition(_draggingEntity, ScreenToWorld(e.Location));
            _canvasPanel.Invalidate();
        }

        private void OnCanvasMouseUp(object? sender, MouseEventArgs e)
        {
            if (_draggingEntity is null) return;

            _draggingEntity = null;
            MarkDirty();
            RefreshStatusLabel();
        }

        private static void SetEntityPosition(object entity, Vector3Data worldPos)
        {
            switch (entity)
            {
                case PlayerData player: player.Spawn = worldPos; break;
                case EnemyData enemy: enemy.Spawn = worldPos; break;
                case ObstacleData obstacle: obstacle.Position = worldPos; break;
                case GearData gear: gear.Position = worldPos; break;
                case DoorData door: door.Position = worldPos; break;
                case SpawnerData spawner: spawner.Position = worldPos; break;
                case HealthKitData healthKit: healthKit.Position = worldPos; break;
                case BarrelData barrel: barrel.Position = worldPos; break;
                case PowerUpData powerUp: powerUp.Position = worldPos; break;
                case HazardData hazard: hazard.Position = worldPos; break;
                case ElectricTileData tile: tile.Position = worldPos; break;
            }
        }

        // --- Borrado (click derecho) ---

        private void HandleRightClick(Point screenPoint)
        {
            if (_activeTool == EditorTool.DefinePatrol)
            {
                if (_selectedEntity is EnemyData selectedEnemy && RemoveNearestPatrolPoint(selectedEnemy, screenPoint))
                {
                    MarkDirty();
                }
                return;
            }

            object? entity = FindEntityAt(screenPoint);
            if (entity is not null && DeleteEntity(entity)) MarkDirty();
        }

        private bool DeleteEntity(object entity)
        {
            if (ReferenceEquals(entity, _player)) _player = null;
            else if (ReferenceEquals(entity, _door)) _door = null;
            else if (entity is EnemyData enemy) { if (!_enemies.Remove(enemy)) return false; }
            else if (entity is ObstacleData obstacle) { if (!_obstacles.Remove(obstacle)) return false; }
            else if (entity is GearData gear) { if (!_gears.Remove(gear)) return false; }
            else if (entity is SpawnerData spawner) { if (!_spawners.Remove(spawner)) return false; }
            else if (entity is HealthKitData healthKit) { if (!_healthKits.Remove(healthKit)) return false; }
            else if (entity is BarrelData barrel) { if (!_barrels.Remove(barrel)) return false; }
            else if (entity is PowerUpData powerUp) { if (!_powerUps.Remove(powerUp)) return false; }
            else if (entity is HazardData hazard) { if (!_hazards.Remove(hazard)) return false; }
            else if (entity is ElectricTileData tile) { if (!_electricTiles.Remove(tile)) return false; }
            else return false;

            if (ReferenceEquals(_selectedEntity, entity))
            {
                _selectedEntity = null;
                RefreshPropertiesPanel();
            }
            return true;
        }

        private static bool RemoveNearestPatrolPoint(EnemyData enemy, Point screenPoint)
        {
            int bestIndex = -1;
            int bestDistSq = int.MaxValue;
            for (int i = 0; i < enemy.PatrolRoute.Count; i++)
            {
                Point p = WorldToScreen(enemy.PatrolRoute[i]);
                int dx = screenPoint.X - p.X;
                int dy = screenPoint.Y - p.Y;
                int distSq = dx * dx + dy * dy;
                if (distSq <= MarkerRadius * MarkerRadius && distSq < bestDistSq)
                {
                    bestDistSq = distSq;
                    bestIndex = i;
                }
            }
            if (bestIndex < 0) return false;
            enemy.PatrolRoute.RemoveAt(bestIndex);
            return true;
        }

        // --- Panel de propiedades ---

        private static int LabelY(int row) => 25 + row * 55;
        private static int InputY(int row) => 45 + row * 55;

        // Alto del panel de propiedades para el caso normal (3 filas). El
        // Random Spawner lo estira con SetPropertiesHeight porque sus siete
        // filas de peso no caben aquí.
        private const int DefaultPropertiesHeight = 300;

        // Cambia el alto del grupo y REUBICA lo que va debajo: el label de
        // estado se colocó al construir el formulario a partir de
        // _propertiesGroup.Bottom, así que si el grupo crece sin más, el
        // panel de pesos le pasa por encima.
        private void SetPropertiesHeight(int height)
        {
            _propertiesGroup.Height = Math.Max(height, 120);
            _statusLabel.Location = new Point(_statusLabel.Location.X, _propertiesGroup.Bottom + 20);
        }

        private void RefreshPropertiesPanel()
        {
            _propertiesGroup.Controls.Clear();
            SetPropertiesHeight(DefaultPropertiesHeight);

            switch (_selectedEntity)
            {
                case PlayerData player:
                    _propertiesGroup.Visible = true;
                    BuildPlayerProperties(player);
                    break;

                case EnemyData enemy:
                    _propertiesGroup.Visible = true;
                    BuildEnemyProperties(enemy);
                    break;

                case ObstacleData obstacle when obstacle.Type == "cylinder":
                    _propertiesGroup.Visible = true;
                    BuildCylinderProperties(obstacle);
                    break;

                case ObstacleData obstacle:
                    _propertiesGroup.Visible = true;
                    BuildObstacleBoxProperties(obstacle);
                    break;

                case DoorData door:
                    _propertiesGroup.Visible = true;
                    BuildHalfExtentsProperties(door.HalfExtents);
                    break;

                case SpawnerData spawner:
                    _propertiesGroup.Visible = true;
                    BuildSpawnerProperties(spawner);
                    break;

                case HazardData hazard:
                    _propertiesGroup.Visible = true;
                    BuildHazardProperties(hazard);
                    break;

                case PowerUpData powerUp:
                    _propertiesGroup.Visible = true;
                    BuildPowerUpProperties(powerUp);
                    break;

                case ElectricTileData tile:
                    _propertiesGroup.Visible = true;
                    BuildElectricTileProperties(tile);
                    break;

                default:
                    // Gear seleccionado (sin propiedades editables todavía,
                    // solo posición), o nada seleccionado.
                    _propertiesGroup.Visible = false;
                    break;
            }
        }

        private void BuildPlayerProperties(PlayerData player)
        {
            var hpInput = new NumericUpDown
            {
                Location = new Point(10, InputY(0)), Width = 160,
                Minimum = 1, Maximum = 1000, DecimalPlaces = 0,
                Value = (decimal)player.MaxHP
            };
            hpInput.ValueChanged += (s, e) => { player.MaxHP = (float)hpInput.Value; MarkDirty(); };

            var speedInput = new NumericUpDown
            {
                Location = new Point(10, InputY(1)), Width = 160,
                Minimum = 0.5m, Maximum = 20, DecimalPlaces = 1, Increment = 0.5m,
                Value = (decimal)player.Speed
            };
            speedInput.ValueChanged += (s, e) => { player.Speed = (float)speedInput.Value; MarkDirty(); };

            var dmgInput = new NumericUpDown
            {
                Location = new Point(10, InputY(2)), Width = 160,
                Minimum = 1, Maximum = 500, DecimalPlaces = 0,
                Value = (decimal)player.AttackDamage
            };
            dmgInput.ValueChanged += (s, e) => { player.AttackDamage = (float)dmgInput.Value; MarkDirty(); };

            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_hp", new Point(10, LabelY(0))));
            _propertiesGroup.Controls.Add(hpInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_speed", new Point(10, LabelY(1))));
            _propertiesGroup.Controls.Add(speedInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_damage", new Point(10, LabelY(2))));
            _propertiesGroup.Controls.Add(dmgInput);
        }

        private void BuildEnemyProperties(EnemyData enemy)
        {
            // Fila 0: Type. Determina si el motor construye este enemigo con
            // los stats de abajo tal cual ("Default") o los ignora y usa
            // EnemyFactory con los del arquetipo (ver LevelLoader.cpp) -- por
            // eso conviene dejar claro con el propio combo cuál manda.
            var typeCombo = new ComboBox
            {
                Location = new Point(10, InputY(0)), Width = 160,
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            typeCombo.Items.AddRange(BuildVariantItems(EnemyVariantNames));
            typeCombo.SelectedIndex = Array.IndexOf(EnemyVariantNames, enemy.Type);
            if (typeCombo.SelectedIndex < 0) typeCombo.SelectedIndex = 0; // Type de un JSON externo que no reconocemos
            typeCombo.SelectedIndexChanged += (s, e) =>
            {
                enemy.Type = ((ComboBoxItem<string>)typeCombo.SelectedItem!).Value;

                // Cambiar de variante actualiza HP/Velocidad/Rango/Daño a los
                // base del arquetipo nuevo -- si no, "Tank" se quedaría con
                // los números de lo que hubiera antes (o los del primer
                // enemigo colocado en la sesión), que es justo lo reportado
                // en el playtest. "Default" (sin entrada en el catálogo) deja
                // los valores tal cual, para poder ajustarlos a mano.
                EnemyVariantStatsData? stats = EnemyVariantCatalog.TryGet(enemy.Type);
                if (stats != null)
                {
                    enemy.MaxHP = stats.MaxHP;
                    enemy.VisionRadius = stats.VisionRadius;
                    enemy.Speed = stats.Speed;
                    enemy.AttackDamage = stats.AttackDamage;
                }

                MarkDirty();
                RefreshPropertiesPanel(); // reconstruye los NumericUpDown con los valores nuevos
            };

            var hpInput = new NumericUpDown
            {
                Location = new Point(10, InputY(1)), Width = 160,
                Minimum = 1, Maximum = 1000, DecimalPlaces = 0,
                Value = (decimal)enemy.MaxHP
            };
            hpInput.ValueChanged += (s, e) => { enemy.MaxHP = (float)hpInput.Value; MarkDirty(); };

            var visionInput = new NumericUpDown
            {
                Location = new Point(10, InputY(2)), Width = 160,
                Minimum = 0, Maximum = 50, DecimalPlaces = 1, Increment = 0.5m,
                Value = (decimal)enemy.VisionRadius
            };
            visionInput.ValueChanged += (s, e) => { enemy.VisionRadius = (float)visionInput.Value; MarkDirty(); };

            var speedInput = new NumericUpDown
            {
                Location = new Point(10, InputY(3)), Width = 160,
                Minimum = 0.5m, Maximum = 20, DecimalPlaces = 1, Increment = 0.5m,
                Value = (decimal)enemy.Speed
            };
            speedInput.ValueChanged += (s, e) => { enemy.Speed = (float)speedInput.Value; MarkDirty(); };

            var dmgInput = new NumericUpDown
            {
                Location = new Point(10, InputY(4)), Width = 160,
                Minimum = 1, Maximum = 500, DecimalPlaces = 0,
                Value = (decimal)enemy.AttackDamage
            };
            dmgInput.ValueChanged += (s, e) => { enemy.AttackDamage = (float)dmgInput.Value; MarkDirty(); };

            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_type", new Point(10, LabelY(0))));
            _propertiesGroup.Controls.Add(typeCombo);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_hp", new Point(10, LabelY(1))));
            _propertiesGroup.Controls.Add(hpInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_vision", new Point(10, LabelY(2))));
            _propertiesGroup.Controls.Add(visionInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_speed", new Point(10, LabelY(3))));
            _propertiesGroup.Controls.Add(speedInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_damage", new Point(10, LabelY(4))));
            _propertiesGroup.Controls.Add(dmgInput);
        }

        // Tabla de pesos por defecto al marcar "Aleatorio": todos los
        // arquetipos presentes, la chusma rápida como base y el Buffer casi
        // testimonial (el motor además solo deja 2 vivos a la vez, ver
        // Spawner::kMaxLiveBuffers). Es un punto de partida razonable para no
        // obligar a teclear siete números desde cero.
        private static Dictionary<string, int> DefaultSpawnerWeights() => new()
        {
            { "Runner", 5 }, { "Spitter", 3 }, { "Kamikaze", 3 },
            { "Tank", 2 }, { "Shielder", 2 }, { "Trapper", 2 }, { "Buffer", 1 },
        };

        // "Default" nunca aparece en un spawner: siempre necesita una variante
        // real de EnemyFactory (ver el mismo criterio en OnCanvasMouseClick).
        private static string[] SpawnerVariants() => EnemyVariantNames.Where(n => n != "Default").ToArray();

        private void BuildSpawnerProperties(SpawnerData spawner)
        {
            var typeCombo = new ComboBox
            {
                Location = new Point(10, InputY(0)), Width = 160,
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            string[] spawnerVariants = SpawnerVariants();
            typeCombo.Items.AddRange(BuildVariantItems(spawnerVariants));
            typeCombo.SelectedIndex = Array.IndexOf(spawnerVariants, spawner.EnemyType);
            if (typeCombo.SelectedIndex < 0) typeCombo.SelectedIndex = 0;
            typeCombo.SelectedIndexChanged += (s, e) => { spawner.EnemyType = ((ComboBoxItem<string>)typeCombo.SelectedItem!).Value; MarkDirty(); };

            var intervalInput = new NumericUpDown
            {
                Location = new Point(10, InputY(1)), Width = 160,
                Minimum = 0.5m, Maximum = 60, DecimalPlaces = 1, Increment = 0.5m,
                Value = (decimal)spawner.Interval
            };
            intervalInput.ValueChanged += (s, e) => { spawner.Interval = (float)intervalInput.Value; MarkDirty(); };

            var maxEnemiesInput = new NumericUpDown
            {
                Location = new Point(10, InputY(2)), Width = 160,
                Minimum = 1, Maximum = 20, DecimalPlaces = 0,
                Value = spawner.MaxEnemies
            };
            maxEnemiesInput.ValueChanged += (s, e) => { spawner.MaxEnemies = (int)maxEnemiesInput.Value; MarkDirty(); };

            // Random Spawner. Marcar la casilla rellena la tabla de pesos por
            // defecto; desmarcarla la borra entera (Weights = null), y el
            // serializador omite la clave, así que el JSON vuelve a ser el de
            // un spawner clásico sin dejar restos.
            var randomCheck = new CheckBox
            {
                Location = new Point(10, LabelY(3)),
                AutoSize = true,
                Checked = spawner.IsRandom
            };
            ApplyLocalizedText(randomCheck, "prop_random");
            randomCheck.CheckedChanged += (s, e) =>
            {
                spawner.Weights = randomCheck.Checked ? DefaultSpawnerWeights() : null;
                MarkDirty();
                RefreshPropertiesPanel(); // muestra/oculta las filas de peso
                _canvasPanel.Invalidate(); // el marcador cambia de color al ser aleatorio
            };

            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_enemytype", new Point(10, LabelY(0))));
            _propertiesGroup.Controls.Add(typeCombo);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_interval", new Point(10, LabelY(1))));
            _propertiesGroup.Controls.Add(intervalInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_maxenemies", new Point(10, LabelY(2))));
            _propertiesGroup.Controls.Add(maxEnemiesInput);
            _propertiesGroup.Controls.Add(randomCheck);

            if (!spawner.IsRandom)
            {
                SetPropertiesHeight(LabelY(3) + 40);
                return;
            }

            // Filas de peso COMPACTAS (etiqueta y campo en la misma línea, 26px
            // de alto): con el paso normal de 55px las siete no cabrían ni de
            // lejos en el panel lateral. Los pesos son relativos, no
            // porcentajes: el motor los normaliza contra su propio total.
            const int compactRowHeight = 26;
            int weightsTop = LabelY(3) + 32;

            for (int i = 0; i < spawnerVariants.Length; i++)
            {
                string variant = spawnerVariants[i];
                int rowY = weightsTop + i * compactRowHeight;

                Label label = CreateLocalizedLabel($"variant_{variant}", new Point(10, rowY + 3));
                label.AutoSize = false;
                label.Size = new Size(92, 20);

                var weightInput = new NumericUpDown
                {
                    Location = new Point(106, rowY), Width = 64,
                    Minimum = 0, Maximum = 100, DecimalPlaces = 0,
                    Value = spawner.Weights!.TryGetValue(variant, out int w) ? w : 0
                };
                weightInput.ValueChanged += (s, e) =>
                {
                    spawner.Weights![variant] = (int)weightInput.Value;
                    MarkDirty();
                };

                _propertiesGroup.Controls.Add(label);
                _propertiesGroup.Controls.Add(weightInput);
            }

            SetPropertiesHeight(weightsTop + spawnerVariants.Length * compactRowHeight + 12);
        }

        // Obstacle tipo "box": Ancho/Alto/Largo editables sobre Size
        // (dimensión completa -- el motor C++ hace halfExtents = size * 0.5).
        private void BuildObstacleBoxProperties(ObstacleData obstacle)
        {
            var widthInput = new NumericUpDown
            {
                Location = new Point(10, InputY(0)), Width = 160,
                Minimum = 0.2m, Maximum = 20, DecimalPlaces = 2, Increment = 0.2m,
                Value = (decimal)obstacle.Size.X
            };
            widthInput.ValueChanged += (s, e) => { obstacle.Size.X = (float)widthInput.Value; MarkDirty(); _canvasPanel.Invalidate(); };

            var heightInput = new NumericUpDown
            {
                Location = new Point(10, InputY(1)), Width = 160,
                Minimum = 0.2m, Maximum = 20, DecimalPlaces = 2, Increment = 0.2m,
                Value = (decimal)obstacle.Size.Y
            };
            heightInput.ValueChanged += (s, e) => { obstacle.Size.Y = (float)heightInput.Value; MarkDirty(); };

            var lengthInput = new NumericUpDown
            {
                Location = new Point(10, InputY(2)), Width = 160,
                Minimum = 0.2m, Maximum = 20, DecimalPlaces = 2, Increment = 0.2m,
                Value = (decimal)obstacle.Size.Z
            };
            lengthInput.ValueChanged += (s, e) => { obstacle.Size.Z = (float)lengthInput.Value; MarkDirty(); _canvasPanel.Invalidate(); };

            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_width", new Point(10, LabelY(0))));
            _propertiesGroup.Controls.Add(widthInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_height", new Point(10, LabelY(1))));
            _propertiesGroup.Controls.Add(heightInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_length", new Point(10, LabelY(2))));
            _propertiesGroup.Controls.Add(lengthInput);
        }

        // Obstacle tipo "cylinder": Radio (afecta al círculo del canvas) y Alto.
        private void BuildCylinderProperties(ObstacleData obstacle)
        {
            var radiusInput = new NumericUpDown
            {
                Location = new Point(10, InputY(0)), Width = 160,
                Minimum = 0.2m, Maximum = 10, DecimalPlaces = 2, Increment = 0.1m,
                Value = (decimal)obstacle.Radius
            };
            radiusInput.ValueChanged += (s, e) => { obstacle.Radius = (float)radiusInput.Value; MarkDirty(); _canvasPanel.Invalidate(); };

            var heightInput = new NumericUpDown
            {
                Location = new Point(10, InputY(1)), Width = 160,
                Minimum = 0.2m, Maximum = 20, DecimalPlaces = 2, Increment = 0.2m,
                Value = (decimal)obstacle.Height
            };
            heightInput.ValueChanged += (s, e) => { obstacle.Height = (float)heightInput.Value; MarkDirty(); };

            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_radius", new Point(10, LabelY(0))));
            _propertiesGroup.Controls.Add(radiusInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_height", new Point(10, LabelY(1))));
            _propertiesGroup.Controls.Add(heightInput);
        }

        // Hazard: Ancho/Largo de la zona (X/Z de Size; el grosor Y se queda
        // fijo y no es editable, es solo una placa fina a ras de suelo) y el
        // daño por tick que aplica CombatSystem::ApplyHazardDamage.
        private void BuildHazardProperties(HazardData hazard)
        {
            var widthInput = new NumericUpDown
            {
                Location = new Point(10, InputY(0)), Width = 160,
                Minimum = 0.5m, Maximum = 20, DecimalPlaces = 2, Increment = 0.5m,
                Value = (decimal)hazard.Size.X
            };
            widthInput.ValueChanged += (s, e) => { hazard.Size.X = (float)widthInput.Value; MarkDirty(); _canvasPanel.Invalidate(); };

            var lengthInput = new NumericUpDown
            {
                Location = new Point(10, InputY(1)), Width = 160,
                Minimum = 0.5m, Maximum = 20, DecimalPlaces = 2, Increment = 0.5m,
                Value = (decimal)hazard.Size.Z
            };
            lengthInput.ValueChanged += (s, e) => { hazard.Size.Z = (float)lengthInput.Value; MarkDirty(); _canvasPanel.Invalidate(); };

            var damageInput = new NumericUpDown
            {
                Location = new Point(10, InputY(2)), Width = 160,
                Minimum = 1, Maximum = 100, DecimalPlaces = 0,
                Value = (decimal)hazard.DamagePerTick
            };
            damageInput.ValueChanged += (s, e) => { hazard.DamagePerTick = (float)damageInput.Value; MarkDirty(); };

            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_width", new Point(10, LabelY(0))));
            _propertiesGroup.Controls.Add(widthInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_length", new Point(10, LabelY(1))));
            _propertiesGroup.Controls.Add(lengthInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_damagepertick", new Point(10, LabelY(2))));
            _propertiesGroup.Controls.Add(damageInput);
        }

        // Baldosa eléctrica: tamaño de la placa, daño de la descarga y cada
        // cuánto se arma sola. CycleInterval = 0 significa "solo al pisarla",
        // así que el NumericUpDown baja hasta 0 a propósito.
        private void BuildElectricTileProperties(ElectricTileData tile)
        {
            var widthInput = new NumericUpDown
            {
                Location = new Point(10, InputY(0)), Width = 160,
                Minimum = 0.5m, Maximum = 20, DecimalPlaces = 2, Increment = 0.5m,
                Value = (decimal)tile.Size.X
            };
            widthInput.ValueChanged += (s, e) => { tile.Size.X = (float)widthInput.Value; MarkDirty(); _canvasPanel.Invalidate(); };

            var lengthInput = new NumericUpDown
            {
                Location = new Point(10, InputY(1)), Width = 160,
                Minimum = 0.5m, Maximum = 20, DecimalPlaces = 2, Increment = 0.5m,
                Value = (decimal)tile.Size.Z
            };
            lengthInput.ValueChanged += (s, e) => { tile.Size.Z = (float)lengthInput.Value; MarkDirty(); _canvasPanel.Invalidate(); };

            var damageInput = new NumericUpDown
            {
                Location = new Point(10, InputY(2)), Width = 160,
                Minimum = 1, Maximum = 200, DecimalPlaces = 0,
                Value = (decimal)tile.Damage
            };
            damageInput.ValueChanged += (s, e) => { tile.Damage = (float)damageInput.Value; MarkDirty(); };

            var cycleInput = new NumericUpDown
            {
                Location = new Point(10, InputY(3)), Width = 160,
                Minimum = 0, Maximum = 60, DecimalPlaces = 1, Increment = 0.5m,
                Value = (decimal)tile.CycleInterval
            };
            cycleInput.ValueChanged += (s, e) => { tile.CycleInterval = (float)cycleInput.Value; MarkDirty(); _canvasPanel.Invalidate(); };

            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_width", new Point(10, LabelY(0))));
            _propertiesGroup.Controls.Add(widthInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_length", new Point(10, LabelY(1))));
            _propertiesGroup.Controls.Add(lengthInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_damage", new Point(10, LabelY(2))));
            _propertiesGroup.Controls.Add(damageInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_cycle", new Point(10, LabelY(3))));
            _propertiesGroup.Controls.Add(cycleInput);
        }

        // Power-Up: solo el tipo de efecto. La duración y la magnitud las
        // fija el motor (Player::kPowerUpDuration y compañía), no el nivel:
        // dos escudos que absorbieran distinto según dónde estén colocados
        // serían imposibles de leer en pantalla.
        private void BuildPowerUpProperties(PowerUpData powerUp)
        {
            var typeCombo = new ComboBox
            {
                Location = new Point(10, InputY(0)), Width = 160,
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            typeCombo.Items.AddRange(BuildPowerUpItems());
            typeCombo.SelectedIndex = Array.IndexOf(PowerUpTypeNames, powerUp.Type);
            if (typeCombo.SelectedIndex < 0) typeCombo.SelectedIndex = 0; // Type de un JSON externo que no reconocemos
            typeCombo.SelectedIndexChanged += (s, e) =>
            {
                powerUp.Type = ((ComboBoxItem<string>)typeCombo.SelectedItem!).Value;
                MarkDirty();
                _canvasPanel.Invalidate(); // el color del marcador depende del tipo
            };

            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_powerup_type", new Point(10, LabelY(0))));
            _propertiesGroup.Controls.Add(typeCombo);
        }

        // Compartido por Door (Obstacle-box ya usa Size, ver
        // BuildObstacleBoxProperties): posición + halfExtents directo.
        private void BuildHalfExtentsProperties(Vector3Data halfExtents)
        {
            var xInput = new NumericUpDown
            {
                Location = new Point(10, InputY(0)), Width = 160,
                Minimum = 0.1m, Maximum = 10, DecimalPlaces = 2, Increment = 0.1m,
                Value = (decimal)halfExtents.X
            };
            xInput.ValueChanged += (s, e) => { halfExtents.X = (float)xInput.Value; MarkDirty(); _canvasPanel.Invalidate(); };

            var zInput = new NumericUpDown
            {
                Location = new Point(10, InputY(1)), Width = 160,
                Minimum = 0.1m, Maximum = 10, DecimalPlaces = 2, Increment = 0.1m,
                Value = (decimal)halfExtents.Z
            };
            zInput.ValueChanged += (s, e) => { halfExtents.Z = (float)zInput.Value; MarkDirty(); _canvasPanel.Invalidate(); };

            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_halfextents_x", new Point(10, LabelY(0))));
            _propertiesGroup.Controls.Add(xInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_halfextents_z", new Point(10, LabelY(1))));
            _propertiesGroup.Controls.Add(zInput);
        }

        // --- Dibujado ---

        private void OnCanvasPaint(object? sender, PaintEventArgs e)
        {
            DrawGrid(e.Graphics);
            DrawPatrolRoutes(e.Graphics);
            DrawHazards(e.Graphics);
            DrawElectricTiles(e.Graphics);
            DrawObstacles(e.Graphics);
            DrawDoor(e.Graphics);
            DrawGears(e.Graphics);
            DrawHealthKits(e.Graphics);
            DrawPowerUps(e.Graphics);
            DrawBarrels(e.Graphics);
            DrawSpawners(e.Graphics);
            DrawEnemies(e.Graphics);
            DrawPlayer(e.Graphics);
            DrawSelectionHighlight(e.Graphics);
        }

        private static void DrawGrid(Graphics g)
        {
            using var gridPen = new Pen(Color.LightGray);
            for (int x = 0; x <= CanvasSize; x += CellSize) g.DrawLine(gridPen, x, 0, x, CanvasSize);
            for (int y = 0; y <= CanvasSize; y += CellSize) g.DrawLine(gridPen, 0, y, CanvasSize, y);

            using var axisPen = new Pen(Color.Gray, 1.5f);
            g.DrawLine(axisPen, CanvasCenter.X, 0, CanvasCenter.X, CanvasSize);
            g.DrawLine(axisPen, 0, CanvasCenter.Y, CanvasSize, CanvasCenter.Y);
        }

        private void DrawPlayer(Graphics g)
        {
            if (_player is null) return;
            DrawEntityMarker(g, Brushes.Blue, _player.Spawn);
        }

        private void DrawEnemies(Graphics g)
        {
            foreach (var enemy in _enemies) DrawEntityMarker(g, Brushes.Red, enemy.Spawn);
        }

        private void DrawObstacles(Graphics g)
        {
            foreach (var obstacle in _obstacles)
            {
                if (obstacle.Type == "cylinder")
                {
                    // Círculo, distinto en forma de un Obstacle-box (rectángulo) --
                    // color propio (SteelBlue) para que se distinga de un vistazo.
                    var (center, radiusPx) = GetCylinderScreenCircle(obstacle);
                    g.FillEllipse(Brushes.SteelBlue, center.X - radiusPx, center.Y - radiusPx, radiusPx * 2, radiusPx * 2);
                }
                else
                {
                    g.FillRectangle(Brushes.DarkSlateGray, GetBoxScreenRect(obstacle.Position, GetObstacleBoxHalfExtents(obstacle)));
                }
            }
        }

        // Zona rayada naranja/amarilla: se distingue tanto de un Obstacle
        // (rectángulo gris sólido, bloquea el paso) como de cualquier
        // marcador circular -- un hazard no bloquea el paso, así que su
        // representación no puede confundirse con la de algo que sí lo hace.
        private void DrawHazards(Graphics g)
        {
            using var hatchBrush = new HatchBrush(HatchStyle.WideDownwardDiagonal, Color.OrangeRed, Color.FromArgb(255, 250, 200));
            using var borderPen = new Pen(Color.OrangeRed, 2.0f);

            foreach (var hazard in _hazards)
            {
                Rectangle rect = GetBoxScreenRect(hazard.Position, GetHazardHalfExtents(hazard));
                g.FillRectangle(hatchBrush, rect);
                g.DrawRectangle(borderPen, rect);
            }
        }

        // Azul acero rayado en vertical, distinto del rayado diagonal
        // naranja del Hazard: los dos son placas de suelo que no bloquean,
        // así que tienen que diferenciarse por patrón Y color, no solo color.
        // Las de ciclo llevan además el intervalo escrito encima -- es el dato
        // que decide si son cruzables o no, y no se ve de ninguna otra forma.
        private void DrawElectricTiles(Graphics g)
        {
            using var hatchBrush = new HatchBrush(HatchStyle.LightVertical, Color.DeepSkyBlue, Color.FromArgb(30, 40, 60));
            using var borderPen = new Pen(Color.Gold, 2.0f);
            using var textBrush = new SolidBrush(Color.Gold);
            using var font = new Font(Font.FontFamily, 7.0f, FontStyle.Bold);

            foreach (var tile in _electricTiles)
            {
                Rectangle rect = GetBoxScreenRect(tile.Position, GetElectricTileHalfExtents(tile));
                g.FillRectangle(hatchBrush, rect);
                g.DrawRectangle(borderPen, rect);

                if (tile.CycleInterval > 0.0f)
                {
                    g.DrawString($"{tile.CycleInterval:0.#}s", font, textBrush, rect.X + 2, rect.Y + 1);
                }
            }
        }

        private void DrawGears(Graphics g)
        {
            foreach (var gear in _gears) DrawEntityMarker(g, Brushes.Orange, gear.Position);
        }

        // Violeta con un anillo exterior para los Random Spawner, magenta liso
        // para los de arquetipo fijo -- misma distinción que hace Spawner::Draw
        // en el motor, para que el mapa se lea igual en el editor y en juego.
        private void DrawSpawners(Graphics g)
        {
            using var randomBrush = new SolidBrush(Color.FromArgb(170, 90, 255));
            using var randomPen = new Pen(Color.FromArgb(170, 90, 255), 2.0f);

            foreach (var spawner in _spawners)
            {
                if (spawner.IsRandom)
                {
                    DrawEntityMarker(g, randomBrush, spawner.Position);
                    Point p = WorldToScreen(spawner.Position);
                    int r = MarkerRadius + 4;
                    g.DrawEllipse(randomPen, p.X - r, p.Y - r, r * 2, r * 2);
                }
                else
                {
                    DrawEntityMarker(g, Brushes.Magenta, spawner.Position);
                }
            }
        }

        // Cuadrado verde: distinto en forma Y color de cualquier otra
        // entidad, no solo color (Gear ya es un círculo naranja).
        private void DrawHealthKits(Graphics g)
        {
            foreach (var healthKit in _healthKits) DrawSquareMarker(g, Brushes.LimeGreen, healthKit.Position);
        }

        // Firebrick, no Red puro: Enemy ya usa un círculo Red -- con el mismo
        // tono serían indistinguibles a golpe de vista pese a ser entidades
        // muy distintas (uno ataca, el otro solo explota si lo golpeas).
        private void DrawBarrels(Graphics g)
        {
            foreach (var barrel in _barrels) DrawEntityMarker(g, Brushes.Firebrick, barrel.Position);
        }

        // Rombo, no círculo ni cuadrado: las dos formas que quedaban libres
        // ya están cogidas (Gear/Spawner/Barrel son círculos, HealthKit es un
        // cuadrado). El color lo pone el tipo, con los MISMOS tonos que
        // PowerUp::TypeColor en el motor C++, para que un power-up se
        // reconozca igual en el editor que en la partida.
        private static Color PowerUpColor(string type) => type switch
        {
            "Frenzy" => Color.FromArgb(255, 130, 40),
            "Shield" => Color.FromArgb(90, 200, 255),
            _ => Color.FromArgb(255, 220, 60), // Overclock, y cualquier tipo no reconocido
        };

        private void DrawPowerUps(Graphics g)
        {
            foreach (var powerUp in _powerUps)
            {
                using var brush = new SolidBrush(PowerUpColor(powerUp.Type));
                g.FillPolygon(brush, GetDiamondPoints(WorldToScreen(powerUp.Position)));
            }
        }

        private static Point[] GetDiamondPoints(Point center) => new[]
        {
            new Point(center.X, center.Y - MarkerRadius),
            new Point(center.X + MarkerRadius, center.Y),
            new Point(center.X, center.Y + MarkerRadius),
            new Point(center.X - MarkerRadius, center.Y),
        };

        private void DrawDoor(Graphics g)
        {
            if (_door is null) return;
            g.FillRectangle(Brushes.Green, GetBoxScreenRect(_door.Position, _door.HalfExtents));
        }

        private const int PatrolPointRadius = 4;

        private void DrawPatrolRoutes(Graphics g)
        {
            using var routePen = new Pen(Color.OrangeRed, 2.0f) { DashStyle = DashStyle.Dash };
            foreach (var enemy in _enemies)
            {
                if (enemy.PatrolRoute.Count < 2) continue;
                for (int i = 0; i < enemy.PatrolRoute.Count - 1; i++)
                {
                    g.DrawLine(routePen, WorldToScreen(enemy.PatrolRoute[i]), WorldToScreen(enemy.PatrolRoute[i + 1]));
                }
            }

            // Puntos visibles para poder apuntar el borrado con click derecho.
            foreach (var enemy in _enemies)
            {
                foreach (var point in enemy.PatrolRoute)
                {
                    Point p = WorldToScreen(point);
                    g.FillEllipse(Brushes.OrangeRed, p.X - PatrolPointRadius, p.Y - PatrolPointRadius, PatrolPointRadius * 2, PatrolPointRadius * 2);
                }
            }
        }

        private void DrawSelectionHighlight(Graphics g)
        {
            if (_selectedEntity is null) return;
            using var highlightPen = new Pen(Color.Cyan, 3.0f);

            switch (_selectedEntity)
            {
                case PlayerData player:
                    DrawMarkerHighlight(g, highlightPen, WorldToScreen(player.Spawn));
                    break;
                case EnemyData enemy:
                    DrawMarkerHighlight(g, highlightPen, WorldToScreen(enemy.Spawn));
                    break;
                case GearData gear:
                    DrawMarkerHighlight(g, highlightPen, WorldToScreen(gear.Position));
                    break;
                case SpawnerData spawner:
                    DrawMarkerHighlight(g, highlightPen, WorldToScreen(spawner.Position));
                    break;
                case HealthKitData healthKit:
                    DrawMarkerHighlight(g, highlightPen, WorldToScreen(healthKit.Position));
                    break;
                case BarrelData barrel:
                    DrawMarkerHighlight(g, highlightPen, WorldToScreen(barrel.Position));
                    break;
                case PowerUpData powerUp:
                    DrawMarkerHighlight(g, highlightPen, WorldToScreen(powerUp.Position));
                    break;
                case ObstacleData obstacle when obstacle.Type == "cylinder":
                    {
                        var (center, radiusPx) = GetCylinderScreenCircle(obstacle);
                        int r = radiusPx + 3;
                        g.DrawEllipse(highlightPen, center.X - r, center.Y - r, r * 2, r * 2);
                        break;
                    }
                case ObstacleData obstacle:
                    {
                        Rectangle rect = GetBoxScreenRect(obstacle.Position, GetObstacleBoxHalfExtents(obstacle));
                        rect.Inflate(3, 3);
                        g.DrawRectangle(highlightPen, rect);
                        break;
                    }
                case DoorData door:
                    {
                        Rectangle rect = GetBoxScreenRect(door.Position, door.HalfExtents);
                        rect.Inflate(3, 3);
                        g.DrawRectangle(highlightPen, rect);
                        break;
                    }
                case HazardData hazard:
                    {
                        Rectangle rect = GetBoxScreenRect(hazard.Position, GetHazardHalfExtents(hazard));
                        rect.Inflate(3, 3);
                        g.DrawRectangle(highlightPen, rect);
                        break;
                    }
                case ElectricTileData tile:
                    {
                        Rectangle rect = GetBoxScreenRect(tile.Position, GetElectricTileHalfExtents(tile));
                        rect.Inflate(3, 3);
                        g.DrawRectangle(highlightPen, rect);
                        break;
                    }
            }
        }

        private static void DrawMarkerHighlight(Graphics g, Pen pen, Point center)
        {
            int r = MarkerRadius + 4;
            g.DrawEllipse(pen, center.X - r, center.Y - r, r * 2, r * 2);
        }

        private static void DrawEntityMarker(Graphics g, Brush brush, Vector3Data worldPos)
        {
            Point p = WorldToScreen(worldPos);
            g.FillEllipse(brush, p.X - MarkerRadius, p.Y - MarkerRadius, MarkerRadius * 2, MarkerRadius * 2);
        }

        private static void DrawSquareMarker(Graphics g, Brush brush, Vector3Data worldPos)
        {
            Point p = WorldToScreen(worldPos);
            g.FillRectangle(brush, p.X - MarkerRadius, p.Y - MarkerRadius, MarkerRadius * 2, MarkerRadius * 2);
        }

        // --- Abrir ---

        private void OnOpenButtonClick(object? sender, EventArgs e)
        {
            if (_isDirty)
            {
                DialogResult confirm = MessageBox.Show(
                    LocalizationManager.GetText("msg_unsaved_body"),
                    LocalizationManager.GetText("msg_unsaved_title"), MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
                if (confirm != DialogResult.Yes) return;
            }

            using var openFileDialog = new OpenFileDialog
            {
                Filter = LocalizationManager.GetText("dialog_json_filter"),
                Title = LocalizationManager.GetText("dialog_open_title")
            };

            if (openFileDialog.ShowDialog() != DialogResult.OK) return;

            try
            {
                LevelData level = LevelFileService.Load(openFileDialog.FileName);

                _levelName = level.LevelName;
                _player = level.Player;
                _enemies.Clear();
                _enemies.AddRange(level.Enemies);
                _obstacles.Clear();
                _obstacles.AddRange(level.Obstacles);

                _gears.Clear();
                _gears.AddRange(level.Gears);
                _spawners.Clear();
                _spawners.AddRange(level.Spawners);
                _healthKits.Clear();
                _healthKits.AddRange(level.HealthKits);
                _barrels.Clear();
                _barrels.AddRange(level.Barrels);
                _hazards.Clear();
                _hazards.AddRange(level.Hazards);
                _powerUps.Clear();
                _powerUps.AddRange(level.PowerUps);
                _electricTiles.Clear();
                _electricTiles.AddRange(level.ElectricTiles);
                _door = level.Door;

                _selectedEntity = null;
                RefreshPropertiesPanel();
                RefreshStatusLabel();
                _canvasPanel.Invalidate();

                _currentFilePath = openFileDialog.FileName;
                _isDirty = false;
                UpdateTitle();
            }
            catch (Exception ex)
            {
                MessageBox.Show(LocalizationManager.GetText("msg_open_error_body", ex.Message), LocalizationManager.GetText("error_title"),
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        // --- Exportar ---

        private void OnExportButtonClick(object? sender, EventArgs e)
        {
            if (_player is null)
            {
                MessageBox.Show(LocalizationManager.GetText("msg_missing_player_body"),
                    LocalizationManager.GetText("msg_missing_player_title"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            var level = new LevelData
            {
                LevelName = _levelName,
                Player = _player,
                Obstacles = _obstacles,
                Enemies = _enemies,
                Gears = _gears,
                Spawners = _spawners,
                HealthKits = _healthKits,
                Barrels = _barrels,
                Hazards = _hazards,
                PowerUps = _powerUps,
                ElectricTiles = _electricTiles,
                Door = _door
            };

            using (SaveFileDialog saveFileDialog = new SaveFileDialog())
            {
                saveFileDialog.Filter = LocalizationManager.GetText("dialog_json_filter");
                saveFileDialog.Title = LocalizationManager.GetText("dialog_save_title");
                // Basado en el nombre de nivel en memoria, no un nombre fijo
                // ("sample_level.json"): ese nombre fijo era justo lo que
                // llenaba engine-cpp/assets/ de niveles de prueba residuales
                // cada vez que se exportaba sin cambiarlo a mano -- el motor
                // C++ solo lee assets/data/level_<N>.json y
                // assets/data/endless.json (ver Application::BuildStoryLevelPath),
                // nunca este archivo por su nombre.
                saveFileDialog.FileName = $"{_levelName}.json";

                if (saveFileDialog.ShowDialog() == DialogResult.OK)
                {
                    try
                    {
                        LevelFileService.Save(level, saveFileDialog.FileName);
                        _currentFilePath = saveFileDialog.FileName;
                        _isDirty = false;
                        UpdateTitle();
                        MessageBox.Show(LocalizationManager.GetText("msg_export_success_body", saveFileDialog.FileName),
                            LocalizationManager.GetText("msg_export_success_title"), MessageBoxButtons.OK, MessageBoxIcon.Information);
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show(LocalizationManager.GetText("msg_export_error_body", ex.Message), LocalizationManager.GetText("error_title"),
                            MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
            }
        }

        private string BuildStatusText()
        {
            string playerStatus = LocalizationManager.GetText(_player is null ? "status_player_unplaced" : "status_player_placed");
            string doorStatus = LocalizationManager.GetText(_door is null ? "status_door_unplaced" : "status_door_placed");

            return LocalizationManager.GetText("status_player", playerStatus) + "\n" +
                   LocalizationManager.GetText("status_enemies", _enemies.Count) + "\n" +
                   LocalizationManager.GetText("status_spawners", _spawners.Count) + "\n" +
                   LocalizationManager.GetText("status_obstacles", _obstacles.Count) + "\n" +
                   LocalizationManager.GetText("status_gears", _gears.Count) + "\n" +
                   LocalizationManager.GetText("status_healthkits", _healthKits.Count) + "\n" +
                   LocalizationManager.GetText("status_barrels", _barrels.Count) + "\n" +
                   LocalizationManager.GetText("status_hazards", _hazards.Count) + "\n" +
                   LocalizationManager.GetText("status_powerups", _powerUps.Count) + "\n" +
                   LocalizationManager.GetText("status_electrictiles", _electricTiles.Count) + "\n" +
                   LocalizationManager.GetText("status_door", doorStatus);
        }

        // Reescribe el texto Y la fuente del label de estado -- necesario
        // porque su contenido cambia de ASCII a no-ASCII (o al revés) cada
        // vez que cambia el idioma o el recuento de entidades se traduce.
        private void RefreshStatusLabel()
        {
            string text = BuildStatusText();
            _statusLabel.Text = text;
            _statusLabel.Font = LocalizationManager.ContainsNonAscii(text)
                ? FontResolver.ResolveCjkFont(Font.Size)
                : Font;
        }
    }
}
