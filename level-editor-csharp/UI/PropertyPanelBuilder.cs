using LevelEditor.Core;
using LevelEditor.Localization;
using LevelEditor.Models;

namespace LevelEditor.UI
{
    // El panel de propiedades de la entidad seleccionada: qué campos enseña
    // cada tipo de entidad y qué escriben esos campos en el modelo.
    //
    // Vivía dentro de MainForm (~450 líneas repartidas en once
    // BuildXxxProperties), que ya cargaba con la construcción de la UI, la
    // herramienta activa, la localización, los diálogos de archivo y el
    // hit-test. Aquí no se toca nada que esté fuera del GroupBox recibido:
    // marcar el nivel como sucio, repintar el lienzo y recolocar lo que va
    // debajo cuando el panel cambia de alto entran por delegados, así que
    // este archivo no sabe que existe un MainForm.
    internal sealed class PropertyPanelBuilder
    {
        private readonly GroupBox _group;

        // Fuente base de la que parten las etiquetas: la del formulario, para
        // poder VOLVER a ella cuando un texto deja de ser CJK al cambiar de
        // idioma (ver LocalizedControls).
        private readonly Font _baseFont;

        private readonly Action _markDirty;
        private readonly Action _invalidateCanvas;
        private readonly Action<int> _setHeight;

        // Entidad que se está mostrando. Se guarda porque dos controles (la
        // variante de un enemigo y la casilla "Aleatorio" de un spawner)
        // cambian QUÉ filas hay que pintar, y reconstruyen el panel entero
        // desde dentro con Rebuild().
        private object? _entity;

        public PropertyPanelBuilder(GroupBox group, Font baseFont, Action markDirty, Action invalidateCanvas, Action<int> setHeight)
        {
            _group = group;
            _baseFont = baseFont;
            _markDirty = markDirty;
            _invalidateCanvas = invalidateCanvas;
            _setHeight = setHeight;
        }

        // Único punto de entrada: muestra las propiedades de `entity`
        // (null, o una entidad sin campos editables, deja el panel oculto).
        public void Show(object? entity)
        {
            _entity = entity;
            Rebuild();
        }

        private Label CreateLabel(string textKey, Point location) =>
            LocalizedControls.CreateLabel(textKey, location, _baseFont);

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

        private static int LabelY(int row) => 25 + row * 55;
        private static int InputY(int row) => 45 + row * 55;

        // Alto del panel para el caso normal (3 filas). El Random Spawner lo
        // estira con _setHeight porque sus siete filas de peso no caben aquí.
        private const int DefaultPropertiesHeight = 300;

        private void Rebuild()
        {
            _group.Controls.Clear();
            _setHeight(DefaultPropertiesHeight);

            switch (_entity)
            {
                case PlayerData player:
                    _group.Visible = true;
                    BuildPlayerProperties(player);
                    break;

                case EnemyData enemy:
                    _group.Visible = true;
                    BuildEnemyProperties(enemy);
                    break;

                case ObstacleData obstacle when obstacle.Type == "cylinder":
                    _group.Visible = true;
                    BuildCylinderProperties(obstacle);
                    break;

                case ObstacleData obstacle:
                    _group.Visible = true;
                    BuildObstacleBoxProperties(obstacle);
                    break;

                case DoorData door:
                    _group.Visible = true;
                    BuildHalfExtentsProperties(door.HalfExtents);
                    break;

                case SpawnerData spawner:
                    _group.Visible = true;
                    BuildSpawnerProperties(spawner);
                    break;

                case HazardData hazard:
                    _group.Visible = true;
                    BuildHazardProperties(hazard);
                    break;

                case PowerUpData powerUp:
                    _group.Visible = true;
                    BuildPowerUpProperties(powerUp);
                    break;

                case ElectricTileData tile:
                    _group.Visible = true;
                    BuildElectricTileProperties(tile);
                    break;

                default:
                    // Gear seleccionado (sin propiedades editables todavía,
                    // solo posición), o nada seleccionado.
                    _group.Visible = false;
                    break;
            }
        }

        private void BuildPlayerProperties(PlayerData player)
        {
            AddRow("prop_hp",     0, 1,    1000, 0, 1,    player.MaxHP,        v => player.MaxHP = v);
            AddRow("prop_speed",  1, 0.5m, 20,   1, 0.5m, player.Speed,        v => player.Speed = v);
            AddRow("prop_damage", 2, 1,    500,  0, 1,    player.AttackDamage, v => player.AttackDamage = v);
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
            typeCombo.Items.AddRange(EnemyVariantCatalog.BuildItems(EnemyVariantCatalog.Names));
            typeCombo.SelectedIndex = Array.IndexOf(EnemyVariantCatalog.Names, enemy.Type);
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

                _markDirty();
                Rebuild(); // reconstruye los NumericUpDown con los valores nuevos
            };

            _group.Controls.Add(CreateLabel("prop_type", new Point(10, LabelY(0))));
            _group.Controls.Add(typeCombo);
            AddRow("prop_hp",     1, 1,    1000, 0, 1,    enemy.MaxHP,        v => enemy.MaxHP = v);
            AddRow("prop_vision", 2, 0,    50,   1, 0.5m, enemy.VisionRadius, v => enemy.VisionRadius = v);
            AddRow("prop_speed",  3, 0.5m, 20,   1, 0.5m, enemy.Speed,        v => enemy.Speed = v);
            AddRow("prop_damage", 4, 1,    500,  0, 1,    enemy.AttackDamage, v => enemy.AttackDamage = v);
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
        private static string[] SpawnerVariants() => EnemyVariantCatalog.Names.Where(n => n != "Default").ToArray();

        private void BuildSpawnerProperties(SpawnerData spawner)
        {
            var typeCombo = new ComboBox
            {
                Location = new Point(10, InputY(0)), Width = 160,
                DropDownStyle = ComboBoxStyle.DropDownList
            };
            string[] spawnerVariants = SpawnerVariants();
            typeCombo.Items.AddRange(EnemyVariantCatalog.BuildItems(spawnerVariants));
            typeCombo.SelectedIndex = Array.IndexOf(spawnerVariants, spawner.EnemyType);
            if (typeCombo.SelectedIndex < 0) typeCombo.SelectedIndex = 0;
            typeCombo.SelectedIndexChanged += (s, e) => { spawner.EnemyType = ((ComboBoxItem<string>)typeCombo.SelectedItem!).Value; _markDirty(); };

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
            LocalizedControls.ApplyText(randomCheck, "prop_random", _baseFont);
            randomCheck.CheckedChanged += (s, e) =>
            {
                spawner.Weights = randomCheck.Checked ? DefaultSpawnerWeights() : null;
                _markDirty();
                Rebuild(); // muestra/oculta las filas de peso
                _invalidateCanvas(); // el marcador cambia de color al ser aleatorio
            };

            _group.Controls.Add(CreateLabel("prop_enemytype", new Point(10, LabelY(0))));
            _group.Controls.Add(typeCombo);
            AddRow("prop_interval",   1, 0.5m, 60, 1, 0.5m, spawner.Interval,   v => spawner.Interval = v);
            AddRow("prop_maxenemies", 2, 1,    20, 0, 1,    spawner.MaxEnemies, v => spawner.MaxEnemies = (int)v);
            _group.Controls.Add(randomCheck);

            if (!spawner.IsRandom)
            {
                _setHeight(LabelY(3) + 40);
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

                Label label = CreateLabel($"variant_{variant}", new Point(10, rowY + 3));
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
                    _markDirty();
                };

                _group.Controls.Add(label);
                _group.Controls.Add(weightInput);
            }

            _setHeight(weightsTop + spawnerVariants.Length * compactRowHeight + 12);
        }

        // Obstacle tipo "box": Ancho/Alto/Largo editables sobre Size
        // (dimensión completa -- el motor C++ hace halfExtents = size * 0.5).
        private void BuildObstacleBoxProperties(ObstacleData obstacle)
        {
            // Ancho y Largo mueven la huella en planta, así que repintan el
            // lienzo; el Alto (Y) no se ve desde arriba.
            AddRow("prop_width",  0, 0.2m, 20, 2, 0.2m, obstacle.Size.X, v => obstacle.Size.X = v, repaint: true);
            AddRow("prop_height", 1, 0.2m, 20, 2, 0.2m, obstacle.Size.Y, v => obstacle.Size.Y = v);
            AddRow("prop_length", 2, 0.2m, 20, 2, 0.2m, obstacle.Size.Z, v => obstacle.Size.Z = v, repaint: true);
        }

        // Obstacle tipo "cylinder": Radio (afecta al círculo del canvas) y Alto.
        private void BuildCylinderProperties(ObstacleData obstacle)
        {
            AddRow("prop_radius", 0, 0.2m, 10, 2, 0.1m, obstacle.Radius, v => obstacle.Radius = v, repaint: true);
            AddRow("prop_height", 1, 0.2m, 20, 2, 0.2m, obstacle.Height, v => obstacle.Height = v);
        }

        // Hazard: Ancho/Largo de la zona (X/Z de Size; el grosor Y se queda
        // fijo y no es editable, es solo una placa fina a ras de suelo) y el
        // daño por tick que aplica CombatSystem::ApplyHazardDamage.
        private void BuildHazardProperties(HazardData hazard)
        {
            AddRow("prop_width",         0, 0.5m, 20,  2, 0.5m, hazard.Size.X,        v => hazard.Size.X = v, repaint: true);
            AddRow("prop_length",        1, 0.5m, 20,  2, 0.5m, hazard.Size.Z,        v => hazard.Size.Z = v, repaint: true);
            AddRow("prop_damagepertick", 2, 1,    100, 0, 1,    hazard.DamagePerTick, v => hazard.DamagePerTick = v);
        }

        // Baldosa eléctrica: tamaño de la placa, daño de la descarga y cada
        // cuánto se arma sola. CycleInterval = 0 significa "solo al pisarla",
        // así que el NumericUpDown baja hasta 0 a propósito.
        private void BuildElectricTileProperties(ElectricTileData tile)
        {
            AddRow("prop_width",  0, 0.5m, 20,  2, 0.5m, tile.Size.X,       v => tile.Size.X = v, repaint: true);
            AddRow("prop_length", 1, 0.5m, 20,  2, 0.5m, tile.Size.Z,       v => tile.Size.Z = v, repaint: true);
            AddRow("prop_damage", 2, 1,    200, 0, 1,    tile.Damage,       v => tile.Damage = v);
            // El intervalo se escribe sobre la baldosa en el lienzo (ver
            // CanvasRenderer), así que cambiarlo obliga a repintar.
            AddRow("prop_cycle",  3, 0,    60,  1, 0.5m, tile.CycleInterval, v => tile.CycleInterval = v, repaint: true);
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
            typeCombo.Items.AddRange(PowerUpCatalog.BuildItems());
            typeCombo.SelectedIndex = Array.IndexOf(PowerUpCatalog.Names, powerUp.Type);
            if (typeCombo.SelectedIndex < 0) typeCombo.SelectedIndex = 0; // Type de un JSON externo que no reconocemos
            typeCombo.SelectedIndexChanged += (s, e) =>
            {
                powerUp.Type = ((ComboBoxItem<string>)typeCombo.SelectedItem!).Value;
                _markDirty();
                _invalidateCanvas(); // el color del marcador depende del tipo
            };

            _group.Controls.Add(CreateLabel("prop_powerup_type", new Point(10, LabelY(0))));
            _group.Controls.Add(typeCombo);
        }

        // Compartido por Door (Obstacle-box ya usa Size, ver
        // BuildObstacleBoxProperties): posición + halfExtents directo.
        private void BuildHalfExtentsProperties(Vector3Data halfExtents)
        {
            AddRow("prop_halfextents_x", 0, 0.1m, 10, 2, 0.1m, halfExtents.X, v => halfExtents.X = v, repaint: true);
            AddRow("prop_halfextents_z", 1, 0.1m, 10, 2, 0.1m, halfExtents.Z, v => halfExtents.Z = v, repaint: true);
        }
    }
}
