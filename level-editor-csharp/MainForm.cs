using System.Drawing.Drawing2D;
using System.Text.Json;
using System.Text.Json.Serialization;
using LevelEditor.Models;

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
            PlaceGear,
            PlaceDoor,
            DefinePatrol,
            Select
        }

        // --- Grid / conversión de coordenadas ---
        private const int CellSize = 30;
        private const int CanvasSize = 600;
        private static readonly Point CanvasCenter = new(CanvasSize / 2, CanvasSize / 2);
        private const int MarkerRadius = 8;

        // --- Estado del nivel en memoria ---
        private PlayerData? _player;
        private readonly List<EnemyData> _enemies = new();
        private readonly List<ObstacleData> _obstacles = new();
        private readonly List<GearData> _gears = new();
        private DoorData? _door; // singular, como el Player: colocarla de nuevo reemplaza la anterior

        private EditorTool _activeTool = EditorTool.PlacePlayer;
        private object? _selectedEntity;

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

        public MainForm()
        {
            ClientSize = new Size(CanvasSize + 240, 700);
            StartPosition = FormStartPosition.CenterScreen;

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

            // --- Herramientas ---
            var toolsGroup = new GroupBox
            {
                Text = "Herramienta activa",
                Location = new Point(toolsX, 20),
                Size = new Size(190, 250)
            };

            var playerRadio = new RadioButton { Text = "Colocar Jugador", Location = new Point(10, 25), AutoSize = true, Checked = true };
            var enemyRadio = new RadioButton { Text = "Colocar Enemigo", Location = new Point(10, 55), AutoSize = true };
            var obstacleRadio = new RadioButton { Text = "Colocar Obstáculo", Location = new Point(10, 85), AutoSize = true };
            var gearRadio = new RadioButton { Text = "Colocar Engranaje", Location = new Point(10, 115), AutoSize = true };
            var doorRadio = new RadioButton { Text = "Colocar Puerta", Location = new Point(10, 145), AutoSize = true };
            var patrolRadio = new RadioButton { Text = "Definir Patrulla", Location = new Point(10, 175), AutoSize = true };
            var selectRadio = new RadioButton { Text = "Seleccionar / Editar", Location = new Point(10, 205), AutoSize = true };

            playerRadio.CheckedChanged += (s, e) => { if (playerRadio.Checked) _activeTool = EditorTool.PlacePlayer; };
            enemyRadio.CheckedChanged += (s, e) => { if (enemyRadio.Checked) _activeTool = EditorTool.PlaceEnemy; };
            obstacleRadio.CheckedChanged += (s, e) => { if (obstacleRadio.Checked) _activeTool = EditorTool.PlaceObstacle; };
            gearRadio.CheckedChanged += (s, e) => { if (gearRadio.Checked) _activeTool = EditorTool.PlaceGear; };
            doorRadio.CheckedChanged += (s, e) => { if (doorRadio.Checked) _activeTool = EditorTool.PlaceDoor; };
            patrolRadio.CheckedChanged += (s, e) => { if (patrolRadio.Checked) _activeTool = EditorTool.DefinePatrol; };
            selectRadio.CheckedChanged += (s, e) => { if (selectRadio.Checked) _activeTool = EditorTool.Select; };

            toolsGroup.Controls.Add(playerRadio);
            toolsGroup.Controls.Add(enemyRadio);
            toolsGroup.Controls.Add(obstacleRadio);
            toolsGroup.Controls.Add(gearRadio);
            toolsGroup.Controls.Add(doorRadio);
            toolsGroup.Controls.Add(patrolRadio);
            toolsGroup.Controls.Add(selectRadio);
            Controls.Add(toolsGroup);

            var hintLabel = new Label
            {
                Text = "Click derecho: borrar entidad\n(o punto de patrulla más cercano)",
                Location = new Point(toolsX, toolsGroup.Bottom + 5),
                Size = new Size(190, 30),
                ForeColor = Color.DimGray
            };
            Controls.Add(hintLabel);

            // --- Abrir / Exportar ---
            var openButton = new Button
            {
                Text = "Abrir Nivel",
                Location = new Point(toolsX, hintLabel.Bottom + 10),
                AutoSize = true
            };
            openButton.Click += OnOpenButtonClick;
            Controls.Add(openButton);

            var exportButton = new Button
            {
                Text = "Exportar Nivel",
                Location = new Point(toolsX, openButton.Bottom + 10),
                AutoSize = true
            };
            exportButton.Click += OnExportButtonClick;
            Controls.Add(exportButton);

            // --- Propiedades (contenido dinámico, ver RefreshPropertiesPanel) ---
            _propertiesGroup = new GroupBox
            {
                Text = "Propiedades",
                Location = new Point(toolsX, exportButton.Bottom + 20),
                Size = new Size(190, 260),
                Visible = false
            };
            Controls.Add(_propertiesGroup);

            // --- Estado ---
            _statusLabel = new Label
            {
                Location = new Point(toolsX, _propertiesGroup.Bottom + 20),
                Size = new Size(190, 100),
                Text = BuildStatusText()
            };
            Controls.Add(_statusLabel);

            UpdateTitle();
        }

        // --- Título / estado de guardado ---

        private void UpdateTitle()
        {
            string fileLabel = _currentFilePath is null ? "sin guardar" : Path.GetFileName(_currentFilePath);
            Text = $"LEVEL-5 Portfolio - Editor de Niveles — {fileLabel}{(_isDirty ? " *" : "")}";
        }

        private void MarkDirty()
        {
            _isDirty = true;
            UpdateTitle();
        }

        // --- Conversión de coordenadas ---

        private static Vector3Data ScreenToWorld(Point screenPoint)
        {
            float worldX = (screenPoint.X - CanvasCenter.X) / (float)CellSize;
            float worldZ = (screenPoint.Y - CanvasCenter.Y) / (float)CellSize;
            return new Vector3Data(worldX, 0.0f, worldZ);
        }

        private static Point WorldToScreen(Vector3Data world)
        {
            int screenX = CanvasCenter.X + (int)Math.Round(world.X * CellSize);
            int screenY = CanvasCenter.Y + (int)Math.Round(world.Z * CellSize);
            return new Point(screenX, screenY);
        }

        // Rect en pantalla de cualquier entidad con posición + halfExtents
        // (Obstacle y Door comparten esta forma). Usado tanto para dibujar
        // como para el hit-test de selección.
        private static Rectangle GetBoxScreenRect(Vector3Data position, Vector3Data halfExtents)
        {
            Point center = WorldToScreen(position);
            int halfWidthPx = Math.Max((int)Math.Round(halfExtents.X * CellSize), MarkerRadius);
            int halfHeightPx = Math.Max((int)Math.Round(halfExtents.Z * CellSize), MarkerRadius);
            return new Rectangle(center.X - halfWidthPx, center.Y - halfHeightPx, halfWidthPx * 2, halfHeightPx * 2);
        }

        // --- Clic en el lienzo ---

        private void OnCanvasMouseClick(object? sender, MouseEventArgs e)
        {
            if (e.Button == MouseButtons.Right)
            {
                HandleRightClick(e.Location);
                _statusLabel.Text = BuildStatusText();
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
                        HalfExtents = new Vector3Data(0.5f, 0.5f, 0.5f)
                    });
                    MarkDirty();
                    break;

                case EditorTool.PlaceGear:
                    _gears.Add(new GearData { Position = worldPos });
                    MarkDirty();
                    break;

                case EditorTool.PlaceDoor:
                    _door = new DoorData { Position = worldPos, HalfExtents = new Vector3Data(1.0f, 1.0f, 1.0f) };
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
                        MessageBox.Show("Por favor, selecciona primero un enemigo (con la herramienta 'Seleccionar / Editar') para añadirle puntos de patrulla.\n\nClick izquierdo: añade un punto. Click derecho: quita el punto más cercano.",
                                        "Sin enemigo seleccionado", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                    }
                    break;

                case EditorTool.Select:
                    _selectedEntity = FindEntityAt(e.Location);
                    RefreshPropertiesPanel();
                    break;
            }

            _statusLabel.Text = BuildStatusText();
            _canvasPanel.Invalidate();
        }

        // Orden inverso al de dibujado (lo último dibujado, arriba del todo,
        // se prueba primero): Player, Enemies, Gears, Door, Obstacles.
        private object? FindEntityAt(Point screenPoint)
        {
            if (_player is not null && IsPointNearMarker(screenPoint, WorldToScreen(_player.Spawn)))
                return _player;

            for (int i = _enemies.Count - 1; i >= 0; i--)
                if (IsPointNearMarker(screenPoint, WorldToScreen(_enemies[i].Spawn)))
                    return _enemies[i];

            for (int i = _gears.Count - 1; i >= 0; i--)
                if (IsPointNearMarker(screenPoint, WorldToScreen(_gears[i].Position)))
                    return _gears[i];

            if (_door is not null && GetBoxScreenRect(_door.Position, _door.HalfExtents).Contains(screenPoint))
                return _door;

            for (int i = _obstacles.Count - 1; i >= 0; i--)
                if (GetBoxScreenRect(_obstacles[i].Position, _obstacles[i].HalfExtents).Contains(screenPoint))
                    return _obstacles[i];

            return null;
        }

        private static bool IsPointNearMarker(Point point, Point markerCenter)
        {
            int dx = point.X - markerCenter.X;
            int dy = point.Y - markerCenter.Y;
            return (dx * dx + dy * dy) <= (MarkerRadius * MarkerRadius);
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
            _statusLabel.Text = BuildStatusText();
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

                case ObstacleData obstacle:
                    _propertiesGroup.Visible = true;
                    BuildHalfExtentsProperties(obstacle.HalfExtents);
                    break;

                case DoorData door:
                    _propertiesGroup.Visible = true;
                    BuildHalfExtentsProperties(door.HalfExtents);
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

            _propertiesGroup.Controls.Add(new Label { Text = "HP máximo:", Location = new Point(10, LabelY(0)), AutoSize = true });
            _propertiesGroup.Controls.Add(hpInput);
            _propertiesGroup.Controls.Add(new Label { Text = "Velocidad:", Location = new Point(10, LabelY(1)), AutoSize = true });
            _propertiesGroup.Controls.Add(speedInput);
            _propertiesGroup.Controls.Add(new Label { Text = "Daño de ataque:", Location = new Point(10, LabelY(2)), AutoSize = true });
            _propertiesGroup.Controls.Add(dmgInput);
        }

        private void BuildEnemyProperties(EnemyData enemy)
        {
            var hpInput = new NumericUpDown
            {
                Location = new Point(10, InputY(0)), Width = 160,
                Minimum = 1, Maximum = 1000, DecimalPlaces = 0,
                Value = (decimal)enemy.MaxHP
            };
            hpInput.ValueChanged += (s, e) => { enemy.MaxHP = (float)hpInput.Value; MarkDirty(); };

            var visionInput = new NumericUpDown
            {
                Location = new Point(10, InputY(1)), Width = 160,
                Minimum = 0, Maximum = 50, DecimalPlaces = 1, Increment = 0.5m,
                Value = (decimal)enemy.VisionRadius
            };
            visionInput.ValueChanged += (s, e) => { enemy.VisionRadius = (float)visionInput.Value; MarkDirty(); };

            var speedInput = new NumericUpDown
            {
                Location = new Point(10, InputY(2)), Width = 160,
                Minimum = 0.5m, Maximum = 20, DecimalPlaces = 1, Increment = 0.5m,
                Value = (decimal)enemy.Speed
            };
            speedInput.ValueChanged += (s, e) => { enemy.Speed = (float)speedInput.Value; MarkDirty(); };

            var dmgInput = new NumericUpDown
            {
                Location = new Point(10, InputY(3)), Width = 160,
                Minimum = 1, Maximum = 500, DecimalPlaces = 0,
                Value = (decimal)enemy.AttackDamage
            };
            dmgInput.ValueChanged += (s, e) => { enemy.AttackDamage = (float)dmgInput.Value; MarkDirty(); };

            _propertiesGroup.Controls.Add(new Label { Text = "HP máximo:", Location = new Point(10, LabelY(0)), AutoSize = true });
            _propertiesGroup.Controls.Add(hpInput);
            _propertiesGroup.Controls.Add(new Label { Text = "Radio de visión:", Location = new Point(10, LabelY(1)), AutoSize = true });
            _propertiesGroup.Controls.Add(visionInput);
            _propertiesGroup.Controls.Add(new Label { Text = "Velocidad:", Location = new Point(10, LabelY(2)), AutoSize = true });
            _propertiesGroup.Controls.Add(speedInput);
            _propertiesGroup.Controls.Add(new Label { Text = "Daño de ataque:", Location = new Point(10, LabelY(3)), AutoSize = true });
            _propertiesGroup.Controls.Add(dmgInput);
        }

        // Compartido por Obstacle y Door: ambos son solo posición + tamaño.
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

            _propertiesGroup.Controls.Add(new Label { Text = "HalfExtents X:", Location = new Point(10, LabelY(0)), AutoSize = true });
            _propertiesGroup.Controls.Add(xInput);
            _propertiesGroup.Controls.Add(new Label { Text = "HalfExtents Z:", Location = new Point(10, LabelY(1)), AutoSize = true });
            _propertiesGroup.Controls.Add(zInput);
        }

        // --- Dibujado ---

        private void OnCanvasPaint(object? sender, PaintEventArgs e)
        {
            DrawGrid(e.Graphics);
            DrawPatrolRoutes(e.Graphics);
            DrawObstacles(e.Graphics);
            DrawDoor(e.Graphics);
            DrawGears(e.Graphics);
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
                g.FillRectangle(Brushes.DarkSlateGray, GetBoxScreenRect(obstacle.Position, obstacle.HalfExtents));
        }

        private void DrawGears(Graphics g)
        {
            foreach (var gear in _gears) DrawEntityMarker(g, Brushes.Orange, gear.Position);
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
                case ObstacleData obstacle:
                    {
                        Rectangle rect = GetBoxScreenRect(obstacle.Position, obstacle.HalfExtents);
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

        // --- Abrir ---

        private void OnOpenButtonClick(object? sender, EventArgs e)
        {
            if (_isDirty)
            {
                DialogResult confirm = MessageBox.Show(
                    "Hay cambios sin exportar en el nivel actual. Si abres otro nivel se perderán.\n\n¿Continuar de todas formas?",
                    "Cambios sin guardar", MessageBoxButtons.YesNo, MessageBoxIcon.Warning);
                if (confirm != DialogResult.Yes) return;
            }

            using var openFileDialog = new OpenFileDialog
            {
                Filter = "Archivos JSON (*.json)|*.json|Todos los archivos (*.*)|*.*",
                Title = "Abrir nivel del motor C++"
            };

            if (openFileDialog.ShowDialog() != DialogResult.OK) return;

            try
            {
                string json = File.ReadAllText(openFileDialog.FileName);
                LevelData? level = JsonSerializer.Deserialize<LevelData>(json)
                    ?? throw new InvalidDataException("El archivo no contiene un nivel válido.");

                _levelName = level.LevelName;
                _player = level.Player;
                _enemies.Clear();
                _enemies.AddRange(level.Enemies);
                _obstacles.Clear();
                _obstacles.AddRange(level.Obstacles);
                _gears.Clear();
                _gears.AddRange(level.Gears);
                _door = level.Door;

                _selectedEntity = null;
                RefreshPropertiesPanel();
                _statusLabel.Text = BuildStatusText();
                _canvasPanel.Invalidate();

                _currentFilePath = openFileDialog.FileName;
                _isDirty = false;
                UpdateTitle();
            }
            catch (Exception ex)
            {
                MessageBox.Show($"No se pudo abrir el nivel:\n{ex.Message}", "Error",
                    MessageBoxButtons.OK, MessageBoxIcon.Error);
            }
        }

        // --- Exportar ---

        private void OnExportButtonClick(object? sender, EventArgs e)
        {
            if (_player is null)
            {
                MessageBox.Show("Coloca al jugador antes de exportar (herramienta \"Colocar Jugador\").",
                    "Falta el jugador", MessageBoxButtons.OK, MessageBoxIcon.Warning);
                return;
            }

            var level = new LevelData
            {
                LevelName = _levelName,
                Player = _player,
                Obstacles = _obstacles,
                Enemies = _enemies,
                Gears = _gears,
                Door = _door
            };

            var options = new JsonSerializerOptions
            {
                WriteIndented = true,
                // Si _door es null, se omite la clave "door" del JSON en vez
                // de escribir "door": null -- LevelLoader.cpp distingue
                // "ausente" (nivel sin puerta) de "presente pero null" (que
                // le haría fallar el parseo al intentar leer position/halfExtents).
                DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
            };
            string json = JsonSerializer.Serialize(level, options);
            using (SaveFileDialog saveFileDialog = new SaveFileDialog())
            {
                saveFileDialog.Filter = "Archivos JSON (*.json)|*.json|Todos los archivos (*.*)|*.*";
                saveFileDialog.Title = "Guardar nivel del motor C++";
                saveFileDialog.FileName = "sample_level.json"; // Nombre esperado por el motor C++

                if (saveFileDialog.ShowDialog() == DialogResult.OK)
                {
                    try
                    {
                        File.WriteAllText(saveFileDialog.FileName, json);
                        _currentFilePath = saveFileDialog.FileName;
                        _isDirty = false;
                        UpdateTitle();
                        MessageBox.Show($"Nivel exportado a:\n{saveFileDialog.FileName}", "Exportación completada",
                            MessageBoxButtons.OK, MessageBoxIcon.Information);
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show($"No se pudo exportar el nivel:\n{ex.Message}", "Error",
                            MessageBoxButtons.OK, MessageBoxIcon.Error);
                    }
                }
            }
        }

        private string BuildStatusText()
        {
            return $"Jugador: {(_player is null ? "sin colocar" : "colocado")}\n" +
                   $"Enemigos: {_enemies.Count}\n" +
                   $"Obstáculos: {_obstacles.Count}\n" +
                   $"Engranajes: {_gears.Count}\n" +
                   $"Puerta: {(_door is null ? "sin colocar" : "colocada")}";
        }
    }
}
