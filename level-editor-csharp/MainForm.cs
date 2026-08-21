using System.Drawing.Drawing2D;
using LevelEditor.Canvas;
using LevelEditor.Core;
using LevelEditor.Localization;
using LevelEditor.Models;
using LevelEditor.UI;
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

        // --- Estado del nivel en memoria ---
        // Una sola escena en vez de once campos sueltos: sus listas, el
        // hit-test, el borrado y el volcado a/desde LevelData viven en
        // EditorScene, que no depende de WinForms (ver Core/EditorScene.cs).
        private readonly EditorScene _scene = new();

        // Se crea una sola vez, no en cada repintado: la usa
        // CanvasRenderer para el intervalo escrito sobre una baldosa de ciclo.
        private readonly Font _tileLabelFont;

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

        // (El nombre lógico del nivel, "levelName", vive ahora en la escena.)

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

        // Quién rellena _propertiesGroup según lo seleccionado (ver
        // UI/PropertyPanelBuilder.cs). Se construye al final del constructor:
        // necesita el GroupBox y el lienzo ya creados.
        private readonly PropertyPanelBuilder _properties;

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
            _tileLabelFont = new Font(Font.FontFamily, 7.0f, FontStyle.Bold);

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
            // El VALOR (EnemyVariantCatalog.Names) es el identificador que consume
            // EnemyFactory en el motor C++ (ver LevelLoader.cpp) y nunca se
            // traduce; el TEXTO visible sí, vía ComboBoxItem (ver
            // EnemyVariantCatalog.BuildItems) -- RefreshVariantCombo lo reetiqueta al
            // cambiar de idioma sin tocar _selectedVariant.
            _variantCombo.Items.AddRange(EnemyVariantCatalog.BuildItems(EnemyVariantCatalog.Names));
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
            _powerUpCombo.Items.AddRange(PowerUpCatalog.BuildItems());
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

            _properties = new PropertyPanelBuilder(_propertiesGroup, Font, MarkDirty,
                                                   () => _canvasPanel.Invalidate(), SetPropertiesHeight);

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
            _variantCombo.Items.AddRange(EnemyVariantCatalog.BuildItems(EnemyVariantCatalog.Names));
            int index = Array.IndexOf(EnemyVariantCatalog.Names, _selectedVariant);
            _variantCombo.SelectedIndex = index >= 0 ? index : 0;
            _variantCombo.SelectedIndexChanged += OnVariantComboChanged;
        }

        // Igual que RefreshVariantCombo, con el mismo desenganche del handler
        // mientras se reconstruyen los Items (ver el comentario de arriba).
        private void RefreshPowerUpCombo()
        {
            _powerUpCombo.SelectedIndexChanged -= OnPowerUpComboChanged;
            _powerUpCombo.Items.Clear();
            _powerUpCombo.Items.AddRange(PowerUpCatalog.BuildItems());
            int index = Array.IndexOf(PowerUpCatalog.Names, _selectedPowerUpType);
            _powerUpCombo.SelectedIndex = index >= 0 ? index : 0;
            _powerUpCombo.SelectedIndexChanged += OnPowerUpComboChanged;
        }

        // Atajo sobre LocalizedControls con la fuente del formulario como
        // base: la comparte con PropertyPanelBuilder, que crea sus etiquetas
        // por su cuenta pero tiene que resolver la fuente igual.
        private void ApplyLocalizedText(Control control, string textKey) =>
            LocalizedControls.ApplyText(control, textKey, Font);

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
                    _scene.Player = new PlayerData { Spawn = worldPos, MaxHP = 100.0f, Speed = 4.0f, AttackDamage = 50.0f };
                    MarkDirty();
                    break;

                case EditorTool.PlaceEnemy:
                    // Stats base del arquetipo elegido (ver EnemyVariantCatalog) si
                    // existe una en enemy_variants.json; si no ("Default", o una
                    // variante que el catálogo no reconoce), los valores de
                    // siempre -- mismo criterio de degradar sin reventar.
                    EnemyVariantStatsData? placedVariantStats = EnemyVariantCatalog.TryGet(_selectedVariant);
                    _scene.Enemies.Add(new EnemyData
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
                    _scene.Obstacles.Add(new ObstacleData
                    {
                        Position = worldPos,
                        Type = "box",
                        Size = new Vector3Data(1.0f, 1.0f, 1.0f)
                    });
                    MarkDirty();
                    break;

                case EditorTool.PlaceCylinder:
                    _scene.Obstacles.Add(new ObstacleData
                    {
                        Position = worldPos,
                        Type = "cylinder",
                        Radius = 0.6f,
                        Height = 2.5f
                    });
                    MarkDirty();
                    break;

                case EditorTool.PlaceHazard:
                    _scene.Hazards.Add(new HazardData
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
                    _scene.ElectricTiles.Add(new ElectricTileData
                    {
                        Position = worldPos,
                        Size = new Vector3Data(2.0f, 0.1f, 2.0f),
                        Damage = 20.0f,
                        CycleInterval = 0.0f
                    });
                    MarkDirty();
                    break;

                case EditorTool.PlaceGear:
                    _scene.Gears.Add(new GearData { Position = worldPos });
                    MarkDirty();
                    break;

                case EditorTool.PlaceHealthKit:
                    _scene.HealthKits.Add(new HealthKitData { Position = worldPos });
                    MarkDirty();
                    break;

                case EditorTool.PlaceBarrel:
                    _scene.Barrels.Add(new BarrelData { Position = worldPos });
                    MarkDirty();
                    break;

                case EditorTool.PlacePowerUp:
                    _scene.PowerUps.Add(new PowerUpData { Position = worldPos, Type = _selectedPowerUpType });
                    MarkDirty();
                    break;

                case EditorTool.PlaceDoor:
                    _scene.Door = new DoorData { Position = worldPos, HalfExtents = new Vector3Data(1.0f, 1.0f, 1.0f) };
                    MarkDirty();
                    break;

                case EditorTool.PlaceSpawner:
                    _scene.Spawners.Add(new SpawnerData
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
                    _selectedEntity = _scene.FindAt(e.Location);
                    RefreshPropertiesPanel();
                    break;
            }

            RefreshStatusLabel();
            _canvasPanel.Invalidate();
        }

        // --- Arrastrar para mover (herramienta Seleccionar, botón izquierdo) ---

        private void OnCanvasMouseDown(object? sender, MouseEventArgs e)
        {
            if (_activeTool != EditorTool.Select || e.Button != MouseButtons.Left) return;

            object? entity = _scene.FindAt(e.Location);
            if (entity is null) return;

            _draggingEntity = entity;
            _selectedEntity = entity;
            RefreshPropertiesPanel();
            _canvasPanel.Invalidate();
        }

        private void OnCanvasMouseMove(object? sender, MouseEventArgs e)
        {
            if (_draggingEntity is null) return;

            EditorScene.SetPosition(_draggingEntity, ScreenToWorld(e.Location));
            _canvasPanel.Invalidate();
        }

        private void OnCanvasMouseUp(object? sender, MouseEventArgs e)
        {
            if (_draggingEntity is null) return;

            _draggingEntity = null;
            MarkDirty();
            RefreshStatusLabel();
        }

        // --- Borrado (click derecho) ---

        private void HandleRightClick(Point screenPoint)
        {
            if (_activeTool == EditorTool.DefinePatrol)
            {
                if (_selectedEntity is EnemyData selectedEnemy && EditorScene.RemoveNearestPatrolPoint(selectedEnemy, screenPoint))
                {
                    MarkDirty();
                }
                return;
            }

            object? entity = _scene.FindAt(screenPoint);
            if (entity is not null && DeleteEntity(entity)) MarkDirty();
        }

        // --- Panel de propiedades ---

        // Cambia el alto del grupo y REUBICA lo que va debajo: el label de
        // estado se colocó al construir el formulario a partir de
        // _propertiesGroup.Bottom, así que si el grupo crece sin más (las
        // siete filas de peso de un Random Spawner), el panel le pasa por
        // encima. PropertyPanelBuilder lo llama por delegado: es lo único
        // fuera del GroupBox que hay que mover cuando cambia de alto.
        private void SetPropertiesHeight(int height)
        {
            _propertiesGroup.Height = Math.Max(height, 120);
            _statusLabel.Location = new Point(_statusLabel.Location.X, _propertiesGroup.Bottom + 20);
        }

        // El contenido del panel lo arma PropertyPanelBuilder (ver
        // UI/PropertyPanelBuilder.cs); desde aquí solo se le dice qué hay
        // seleccionado.
        private void RefreshPropertiesPanel() => _properties.Show(_selectedEntity);

        // --- Dibujado (delegado en CanvasRenderer) ---

        private void OnCanvasPaint(object? sender, PaintEventArgs e)
        {
            CanvasRenderer.Draw(e.Graphics, _scene, _selectedEntity, _tileLabelFont);
        }

        // Borra la entidad de la escena y, si era la seleccionada, limpia la
        // selección para que el panel de propiedades no siga apuntando a algo
        // que ya no existe.
        private bool DeleteEntity(object entity)
        {
            if (!_scene.Remove(entity)) return false;

            if (ReferenceEquals(_selectedEntity, entity))
            {
                _selectedEntity = null;
                RefreshPropertiesPanel();
            }
            return true;
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
                _scene.LoadFrom(LevelFileService.Load(openFileDialog.FileName));

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
            if (_scene.Player is null)
            {
                MessageBox.Show(LocalizationManager.GetText("msg_missing_player_body"),
                    LocalizationManager.GetText("msg_missing_player_title"), MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            // La escena es el único punto de traducción a/desde LevelData
            // (la comprobación de Player de arriba es justo la precondición
            // que documenta ToLevelData).
            LevelData level = _scene.ToLevelData();

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
                saveFileDialog.FileName = $"{_scene.LevelName}.json";

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
            string playerStatus = LocalizationManager.GetText(_scene.Player is null ? "status_player_unplaced" : "status_player_placed");
            string doorStatus = LocalizationManager.GetText(_scene.Door is null ? "status_door_unplaced" : "status_door_placed");

            return LocalizationManager.GetText("status_player", playerStatus) + "\n" +
                   LocalizationManager.GetText("status_enemies", _scene.Enemies.Count) + "\n" +
                   LocalizationManager.GetText("status_spawners", _scene.Spawners.Count) + "\n" +
                   LocalizationManager.GetText("status_obstacles", _scene.Obstacles.Count) + "\n" +
                   LocalizationManager.GetText("status_gears", _scene.Gears.Count) + "\n" +
                   LocalizationManager.GetText("status_healthkits", _scene.HealthKits.Count) + "\n" +
                   LocalizationManager.GetText("status_barrels", _scene.Barrels.Count) + "\n" +
                   LocalizationManager.GetText("status_hazards", _scene.Hazards.Count) + "\n" +
                   LocalizationManager.GetText("status_powerups", _scene.PowerUps.Count) + "\n" +
                   LocalizationManager.GetText("status_electrictiles", _scene.ElectricTiles.Count) + "\n" +
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
