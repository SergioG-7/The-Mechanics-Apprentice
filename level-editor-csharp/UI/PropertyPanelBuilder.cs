using LevelEditor.Core;
using LevelEditor.Localization;
using LevelEditor.Models;

namespace LevelEditor.UI
{
    // Construye y actualiza el panel de propiedades de la entidad seleccionada.
    internal sealed class PropertyPanelBuilder
    {
        private readonly GroupBox _group;
        private readonly Font _baseFont;
        private readonly Action _markDirty;
        private readonly Action _invalidateCanvas;
        private readonly Action<int> _setHeight;

        // Entidad que se está mostrando actualmente en el panel.
        private object? _entity;

        public PropertyPanelBuilder(GroupBox group, Font baseFont, Action markDirty, Action invalidateCanvas, Action<int> setHeight)
        {
            _group = group;
            _baseFont = baseFont;
            _markDirty = markDirty;
            _invalidateCanvas = invalidateCanvas;
            _setHeight = setHeight;
        }

        // Muestra las propiedades de la entidad dada, u oculta el panel si no hay ninguna.
        public void Show(object? entity)
        {
            _entity = entity;
            Rebuild();
        }

        private Label CreateLabel(string textKey, Point location) =>
            LocalizedControls.CreateLabel(textKey, location, _baseFont);

        // Añade una fila del panel: su etiqueta traducida y el campo numérico que edita el modelo.
        private void AddRow(string textKey, int row, decimal min, decimal max, int decimals, decimal step,
                            float value, Action<float> apply, bool repaint = false)
        {
            // El valor se recorta al rango permitido para no reventar con datos fuera de rango.
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

        private const int DefaultPropertiesHeight = 300;

        // Reconstruye el panel de propiedades según el tipo de entidad seleccionada.
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
                    // Entidad sin propiedades editables, o nada seleccionado.
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

                // Cambiar de variante actualiza HP/Velocidad/Rango/Daño a los valores base del arquetipo.
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

        // Pesos de partida al marcar "Aleatorio", para no tener que teclear los siete a mano.
        private static Dictionary<string, int> DefaultSpawnerWeights() => new()
        {
            { "Runner", 5 }, { "Spitter", 3 }, { "Kamikaze", 3 },
            { "Tank", 2 }, { "Shielder", 2 }, { "Trapper", 2 }, { "Buffer", 1 },
        };

        // Un spawner nunca genera enemigos de tipo "Default", solo variantes reales.
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

            // Marcar la casilla activa el spawner aleatorio con pesos por defecto; desmarcarla lo desactiva.
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

            // Filas compactas para que quepan los siete pesos en el panel lateral.
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

        // Propiedades de un obstáculo tipo bloque: ancho, alto y largo.
        private void BuildObstacleBoxProperties(ObstacleData obstacle)
        {
            AddRow("prop_width",  0, 0.2m, 20, 2, 0.2m, obstacle.Size.X, v => obstacle.Size.X = v, repaint: true);
            AddRow("prop_height", 1, 0.2m, 20, 2, 0.2m, obstacle.Size.Y, v => obstacle.Size.Y = v);
            AddRow("prop_length", 2, 0.2m, 20, 2, 0.2m, obstacle.Size.Z, v => obstacle.Size.Z = v, repaint: true);
        }

        // Propiedades de un obstáculo tipo pilar: radio y alto.
        private void BuildCylinderProperties(ObstacleData obstacle)
        {
            AddRow("prop_radius", 0, 0.2m, 10, 2, 0.1m, obstacle.Radius, v => obstacle.Radius = v, repaint: true);
            AddRow("prop_height", 1, 0.2m, 20, 2, 0.2m, obstacle.Height, v => obstacle.Height = v);
        }

        // Propiedades de una trampa: ancho, largo y daño por tick.
        private void BuildHazardProperties(HazardData hazard)
        {
            AddRow("prop_width",         0, 0.5m, 20,  2, 0.5m, hazard.Size.X,        v => hazard.Size.X = v, repaint: true);
            AddRow("prop_length",        1, 0.5m, 20,  2, 0.5m, hazard.Size.Z,        v => hazard.Size.Z = v, repaint: true);
            AddRow("prop_damagepertick", 2, 1,    100, 0, 1,    hazard.DamagePerTick, v => hazard.DamagePerTick = v);
        }

        // Propiedades de una baldosa eléctrica: tamaño, daño y cada cuánto se arma sola.
        private void BuildElectricTileProperties(ElectricTileData tile)
        {
            AddRow("prop_width",  0, 0.5m, 20,  2, 0.5m, tile.Size.X,       v => tile.Size.X = v, repaint: true);
            AddRow("prop_length", 1, 0.5m, 20,  2, 0.5m, tile.Size.Z,       v => tile.Size.Z = v, repaint: true);
            AddRow("prop_damage", 2, 1,    200, 0, 1,    tile.Damage,       v => tile.Damage = v);
            AddRow("prop_cycle",  3, 0,    60,  1, 0.5m, tile.CycleInterval, v => tile.CycleInterval = v, repaint: true);
        }

        // Propiedades de un power-up: solo su tipo de efecto.
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

        // Propiedades de una puerta: ancho y largo en halfExtents.
        private void BuildHalfExtentsProperties(Vector3Data halfExtents)
        {
            AddRow("prop_halfextents_x", 0, 0.1m, 10, 2, 0.1m, halfExtents.X, v => halfExtents.X = v, repaint: true);
            AddRow("prop_halfextents_z", 1, 0.1m, 10, 2, 0.1m, halfExtents.Z, v => halfExtents.Z = v, repaint: true);
        }
    }
}
