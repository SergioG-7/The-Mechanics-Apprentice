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
            PlaceGear,
            PlaceHealthKit,
            PlaceBarrel,
            PlaceDoor,
            PlaceSpawner,
            DefinePatrol,
            Select
        }

        // Arquetipos disponibles para el ComboBox de variante y para el tipo
        // de enemigo de un Spawner. "Default" es el único que NO pasa por
        // EnemyFactory en el motor C++: usa los stats propios del EnemyData.
        private static readonly string[] EnemyVariantNames = { "Default", "Tank", "Runner", "Spitter", "Kamikaze" };

        // --- Estado del nivel en memoria ---
        private PlayerData? _player;
        private readonly List<EnemyData> _enemies = new();
        private readonly List<ObstacleData> _obstacles = new();
        private readonly List<GearData> _gears = new();
        private readonly List<SpawnerData> _spawners = new();
        private readonly List<HealthKitData> _healthKits = new();
        private readonly List<BarrelData> _barrels = new();
        private readonly List<HazardData> _hazards = new();
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
        private readonly GroupBox _languageGroup;
        private readonly Label _hintLabel;
        private readonly Button _openButton;
        private readonly Button _exportButton;

        public MainForm()
        {
            ClientSize = new Size(CanvasSize + 240, 700);
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
                Size = new Size(190, 220) // 220, no 190: la pestaña Entidades pasó de 4 a 6 herramientas
            };

            _entitiesTab = new TabPage(LocalizationManager.GetText("tab_entities"));
            CreateToolRadio("tool_place_player", 10, EditorTool.PlacePlayer, _entitiesTab, startChecked: true);
            CreateToolRadio("tool_place_enemy", 40, EditorTool.PlaceEnemy, _entitiesTab);
            CreateToolRadio("tool_place_obstacle", 70, EditorTool.PlaceObstacle, _entitiesTab);
            CreateToolRadio("tool_place_cylinder", 100, EditorTool.PlaceCylinder, _entitiesTab);
            CreateToolRadio("tool_place_hazard", 130, EditorTool.PlaceHazard, _entitiesTab);
            CreateToolRadio("tool_place_door", 160, EditorTool.PlaceDoor, _entitiesTab);
            toolTabs.TabPages.Add(_entitiesTab);

            _objectsTab = new TabPage(LocalizationManager.GetText("tab_objects"));
            CreateToolRadio("tool_place_gear", 10, EditorTool.PlaceGear, _objectsTab);
            CreateToolRadio("tool_place_healthkit", 40, EditorTool.PlaceHealthKit, _objectsTab);
            CreateToolRadio("tool_place_barrel", 70, EditorTool.PlaceBarrel, _objectsTab);
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
            var variantCombo = new ComboBox
            {
                Location = new Point(10, 20),
                Width = 160,
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            // Identificadores consumidos por EnemyFactory en el motor C++
            // (ver LevelLoader.cpp), no texto de interfaz -- no se traducen.
            variantCombo.Items.AddRange(EnemyVariantNames);
            variantCombo.SelectedIndex = 0;
            variantCombo.SelectedIndexChanged += (s, e) => _selectedVariant = (string)variantCombo.SelectedItem!;
            _variantGroup.Controls.Add(variantCombo);
            Controls.Add(_variantGroup);

            _hintLabel = new Label
            {
                Text = LocalizationManager.GetText("hint_delete"),
                Location = new Point(toolsX, _variantGroup.Bottom + 5),
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
                Size = new Size(190, 145) // 145, no 130: BuildStatusText tiene 8 líneas desde que se añadieron los Hazards
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
                    _enemies.Add(new EnemyData
                    {
                        Spawn = worldPos,
                        Type = _selectedVariant,
                        MaxHP = 30.0f,
                        VisionRadius = 6.0f,
                        Speed = 2.5f,
                        AttackDamage = 10.0f,
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
        // se prueba primero): Player, Enemies, Spawners, Barrels, HealthKits,
        // Gears, Hazards, Door, Obstacles (Obstacle-box y Cylinder mezclados).
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

            for (int i = _healthKits.Count - 1; i >= 0; i--)
                if (IsPointNearMarker(screenPoint, WorldToScreen(_healthKits[i].Position)))
                    return _healthKits[i];

            for (int i = _gears.Count - 1; i >= 0; i--)
                if (IsPointNearMarker(screenPoint, WorldToScreen(_gears[i].Position)))
                    return _gears[i];

            for (int i = _hazards.Count - 1; i >= 0; i--)
                if (GetBoxScreenRect(_hazards[i].Position, GetHazardHalfExtents(_hazards[i])).Contains(screenPoint))
                    return _hazards[i];

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
                case HazardData hazard: hazard.Position = worldPos; break;
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
            else if (entity is HazardData hazard) { if (!_hazards.Remove(hazard)) return false; }
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

        private void RefreshPropertiesPanel()
        {
            _propertiesGroup.Controls.Clear();

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
            typeCombo.Items.AddRange(EnemyVariantNames);
            typeCombo.SelectedItem = enemy.Type;
            if (typeCombo.SelectedIndex < 0) typeCombo.SelectedIndex = 0; // Type de un JSON externo que no reconocemos
            typeCombo.SelectedIndexChanged += (s, e) => { enemy.Type = (string)typeCombo.SelectedItem!; MarkDirty(); };

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

        private void BuildSpawnerProperties(SpawnerData spawner)
        {
            var typeCombo = new ComboBox
            {
                Location = new Point(10, InputY(0)), Width = 160,
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            // "Default" no aparece aquí: un Spawner siempre necesita una
            // variante real de EnemyFactory, no tiene sentido para él (ver
            // el mismo criterio en OnCanvasMouseClick al colocarlo).
            typeCombo.Items.AddRange(EnemyVariantNames.Where(n => n != "Default").ToArray());
            typeCombo.SelectedItem = spawner.EnemyType;
            if (typeCombo.SelectedIndex < 0) typeCombo.SelectedIndex = 0;
            typeCombo.SelectedIndexChanged += (s, e) => { spawner.EnemyType = (string)typeCombo.SelectedItem!; MarkDirty(); };

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

            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_enemytype", new Point(10, LabelY(0))));
            _propertiesGroup.Controls.Add(typeCombo);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_interval", new Point(10, LabelY(1))));
            _propertiesGroup.Controls.Add(intervalInput);
            _propertiesGroup.Controls.Add(CreateLocalizedLabel("prop_maxenemies", new Point(10, LabelY(2))));
            _propertiesGroup.Controls.Add(maxEnemiesInput);
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
            DrawObstacles(e.Graphics);
            DrawDoor(e.Graphics);
            DrawGears(e.Graphics);
            DrawHealthKits(e.Graphics);
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

        private void DrawGears(Graphics g)
        {
            foreach (var gear in _gears) DrawEntityMarker(g, Brushes.Orange, gear.Position);
        }

        private void DrawSpawners(Graphics g)
        {
            foreach (var spawner in _spawners) DrawEntityMarker(g, Brushes.Magenta, spawner.Position);
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
                Door = _door
            };

            using (SaveFileDialog saveFileDialog = new SaveFileDialog())
            {
                saveFileDialog.Filter = LocalizationManager.GetText("dialog_json_filter");
                saveFileDialog.Title = LocalizationManager.GetText("dialog_save_title");
                saveFileDialog.FileName = "sample_level.json"; // Nombre esperado por el motor C++

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
