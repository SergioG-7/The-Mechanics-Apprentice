using System.Drawing.Drawing2D;
using LevelEditor.Core;
using LevelEditor.Models;
using static LevelEditor.Canvas.CanvasGeometry;

namespace LevelEditor.Canvas
{
    // Todo el pintado del lienzo 2D del editor. Vivía dentro de MainForm (~290
    // líneas de veinte métodos Draw*), que ya cargaba con la construcción de
    // la UI, la herramienta activa, el panel de propiedades, la localización y
    // los diálogos de archivo.
    //
    // Es estático y sin estado: recibe la escena y qué hay seleccionado, y
    // pinta. Nada de lo que hay aquí puede modificar el nivel, que es
    // justamente la garantía que se quería -- el dibujado no muta el modelo.
    internal static class CanvasRenderer
    {
        // labelFont: solo para el intervalo de ciclo escrito sobre una baldosa.
        // Se pasa desde fuera en vez de crearlo aquí para no construir (y
        // destruir) una Font en cada repintado.
        public static void Draw(Graphics g, EditorScene scene, object? selected, Font labelFont)
        {
            DrawGrid(g);
            DrawPatrolRoutes(g, scene);
            DrawHazards(g, scene);
            DrawElectricTiles(g, scene, labelFont);
            DrawObstacles(g, scene);
            DrawDoor(g, scene);
            DrawGears(g, scene);
            DrawHealthKits(g, scene);
            DrawPowerUps(g, scene);
            DrawBarrels(g, scene);
            DrawSpawners(g, scene);
            DrawEnemies(g, scene);
            DrawPlayer(g, scene);
            DrawSelectionHighlight(g, selected);
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

        private static void DrawPlayer(Graphics g, EditorScene scene)
        {
            if (scene.Player is null) return;
            DrawEntityMarker(g, Brushes.Blue, scene.Player.Spawn);
        }

        private static void DrawEnemies(Graphics g, EditorScene scene)
        {
            foreach (var enemy in scene.Enemies) DrawEntityMarker(g, Brushes.Red, enemy.Spawn);
        }

        private static void DrawObstacles(Graphics g, EditorScene scene)
        {
            foreach (var obstacle in scene.Obstacles)
            {
                if (obstacle.Type == "cylinder")
                {
                    // Círculo, distinto en FORMA de un Obstacle-box (rectángulo)
                    // -- color propio (SteelBlue) para que se distinga de un vistazo.
                    var (center, radiusPx) = GetCylinderScreenCircle(obstacle);
                    g.FillEllipse(Brushes.SteelBlue, center.X - radiusPx, center.Y - radiusPx, radiusPx * 2, radiusPx * 2);
                }
                else
                {
                    g.FillRectangle(Brushes.DarkSlateGray, GetBoxScreenRect(obstacle.Position, GetObstacleBoxHalfExtents(obstacle)));
                }
            }
        }

        // Zona rayada naranja: se distingue tanto de un Obstacle (rectángulo
        // gris sólido, bloquea el paso) como de cualquier marcador circular --
        // un hazard no bloquea, así que no puede parecerse a algo que sí lo hace.
        private static void DrawHazards(Graphics g, EditorScene scene)
        {
            using var hatchBrush = new HatchBrush(HatchStyle.WideDownwardDiagonal, Color.OrangeRed, Color.FromArgb(255, 250, 200));
            using var borderPen = new Pen(Color.OrangeRed, 2.0f);

            foreach (var hazard in scene.Hazards)
            {
                Rectangle rect = GetBoxScreenRect(hazard.Position, GetHazardHalfExtents(hazard));
                g.FillRectangle(hatchBrush, rect);
                g.DrawRectangle(borderPen, rect);
            }
        }

        // Azul acero rayado en VERTICAL, distinto del rayado diagonal naranja
        // del Hazard: los dos son placas de suelo que no bloquean, así que
        // tienen que diferenciarse por patrón y color, no solo color. Las de
        // ciclo llevan el intervalo escrito encima -- es el dato que decide si
        // son cruzables, y no se ve de ninguna otra forma.
        private static void DrawElectricTiles(Graphics g, EditorScene scene, Font labelFont)
        {
            using var hatchBrush = new HatchBrush(HatchStyle.LightVertical, Color.DeepSkyBlue, Color.FromArgb(30, 40, 60));
            using var borderPen = new Pen(Color.Gold, 2.0f);
            using var textBrush = new SolidBrush(Color.Gold);

            foreach (var tile in scene.ElectricTiles)
            {
                Rectangle rect = GetBoxScreenRect(tile.Position, GetElectricTileHalfExtents(tile));
                g.FillRectangle(hatchBrush, rect);
                g.DrawRectangle(borderPen, rect);

                if (tile.CycleInterval > 0.0f)
                {
                    g.DrawString($"{tile.CycleInterval:0.#}s", labelFont, textBrush, rect.X + 2, rect.Y + 1);
                }
            }
        }

        private static void DrawGears(Graphics g, EditorScene scene)
        {
            foreach (var gear in scene.Gears) DrawEntityMarker(g, Brushes.Orange, gear.Position);
        }

        // Violeta con anillo exterior para los Random Spawner, magenta liso
        // para los de arquetipo fijo -- misma distinción que hace Spawner::Draw
        // en el motor, para que el mapa se lea igual en el editor y en juego.
        private static void DrawSpawners(Graphics g, EditorScene scene)
        {
            using var randomBrush = new SolidBrush(Color.FromArgb(170, 90, 255));
            using var randomPen = new Pen(Color.FromArgb(170, 90, 255), 2.0f);

            foreach (var spawner in scene.Spawners)
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

        // Cuadrado verde: distinto en FORMA y color de cualquier otra entidad,
        // no solo color (Gear ya es un círculo naranja).
        private static void DrawHealthKits(Graphics g, EditorScene scene)
        {
            foreach (var healthKit in scene.HealthKits) DrawSquareMarker(g, Brushes.LimeGreen, healthKit.Position);
        }

        // Firebrick, no Red puro: Enemy ya usa un círculo Red -- con el mismo
        // tono serían indistinguibles pese a ser entidades muy distintas.
        private static void DrawBarrels(Graphics g, EditorScene scene)
        {
            foreach (var barrel in scene.Barrels) DrawEntityMarker(g, Brushes.Firebrick, barrel.Position);
        }

        // Rombo: las formas de círculo y cuadrado ya estaban cogidas. El color
        // lo pone el tipo, con los MISMOS tonos que PowerUp::TypeColor en el
        // motor, para que se reconozca igual en el editor que en la partida.
        public static Color PowerUpColor(string type) => type switch
        {
            "Frenzy" => Color.FromArgb(255, 130, 40),
            "Shield" => Color.FromArgb(90, 200, 255),
            _ => Color.FromArgb(255, 220, 60), // Overclock, y cualquier tipo no reconocido
        };

        private static void DrawPowerUps(Graphics g, EditorScene scene)
        {
            foreach (var powerUp in scene.PowerUps)
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

        private static void DrawDoor(Graphics g, EditorScene scene)
        {
            if (scene.Door is null) return;
            g.FillRectangle(Brushes.Green, GetBoxScreenRect(scene.Door.Position, scene.Door.HalfExtents));
        }

        private const int PatrolPointRadius = 4;

        private static void DrawPatrolRoutes(Graphics g, EditorScene scene)
        {
            using var routePen = new Pen(Color.OrangeRed, 2.0f) { DashStyle = DashStyle.Dash };
            foreach (var enemy in scene.Enemies)
            {
                if (enemy.PatrolRoute.Count < 2) continue;
                for (int i = 0; i < enemy.PatrolRoute.Count - 1; i++)
                {
                    g.DrawLine(routePen, WorldToScreen(enemy.PatrolRoute[i]), WorldToScreen(enemy.PatrolRoute[i + 1]));
                }
            }

            // Puntos visibles para poder apuntar el borrado con click derecho.
            foreach (var enemy in scene.Enemies)
            {
                foreach (var point in enemy.PatrolRoute)
                {
                    Point p = WorldToScreen(point);
                    g.FillEllipse(Brushes.OrangeRed, p.X - PatrolPointRadius, p.Y - PatrolPointRadius, PatrolPointRadius * 2, PatrolPointRadius * 2);
                }
            }
        }

        private static void DrawSelectionHighlight(Graphics g, object? selected)
        {
            if (selected is null) return;
            using var highlightPen = new Pen(Color.Cyan, 3.0f);

            // Las entidades de marcador comparten resaltado circular; las de
            // placa/caja, uno rectangular inflado. Se agrupan por FORMA, no por
            // tipo, que es lo que evitaba una rama por clase.
            Vector3Data? markerAt = selected switch
            {
                PlayerData player => player.Spawn,
                EnemyData enemy => enemy.Spawn,
                GearData gear => gear.Position,
                SpawnerData spawner => spawner.Position,
                HealthKitData healthKit => healthKit.Position,
                BarrelData barrel => barrel.Position,
                PowerUpData powerUp => powerUp.Position,
                _ => null
            };
            if (markerAt is not null)
            {
                int r = MarkerRadius + 4;
                Point center = WorldToScreen(markerAt);
                g.DrawEllipse(highlightPen, center.X - r, center.Y - r, r * 2, r * 2);
                return;
            }

            if (selected is ObstacleData { Type: "cylinder" } cylinder)
            {
                var (center, radiusPx) = GetCylinderScreenCircle(cylinder);
                int r = radiusPx + 3;
                g.DrawEllipse(highlightPen, center.X - r, center.Y - r, r * 2, r * 2);
                return;
            }

            Rectangle? boxAt = selected switch
            {
                ObstacleData obstacle => GetBoxScreenRect(obstacle.Position, GetObstacleBoxHalfExtents(obstacle)),
                DoorData door => GetBoxScreenRect(door.Position, door.HalfExtents),
                HazardData hazard => GetBoxScreenRect(hazard.Position, GetHazardHalfExtents(hazard)),
                ElectricTileData tile => GetBoxScreenRect(tile.Position, GetElectricTileHalfExtents(tile)),
                _ => null
            };
            if (boxAt is Rectangle rect)
            {
                rect.Inflate(3, 3);
                g.DrawRectangle(highlightPen, rect);
            }
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
    }
}
