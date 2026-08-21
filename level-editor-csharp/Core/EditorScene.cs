using LevelEditor.Models;
using static LevelEditor.Canvas.CanvasGeometry;

namespace LevelEditor.Core
{
    // El nivel que se está editando: todas sus listas y las operaciones que
    // dependen SOLO de ellas (hit-test, borrado, reposicionado, carga y
    // volcado a LevelData).
    //
    // Estaba todo dentro de MainForm, que ya cargaba con la construcción de la
    // UI, la selección de herramienta, el panel de propiedades, la
    // localización, los diálogos de archivo y el dibujado -- once campos de
    // estado más su lógica, mezclados con WinForms. Aquí no se incluye ni un
    // solo `using System.Windows.Forms`: es el modelo, y se puede razonar (o
    // probar) sin levantar un formulario.
    public sealed class EditorScene
    {
        // Nombre lógico del nivel (campo "levelName" del JSON). Se conserva al
        // abrir uno existente para no perderlo al reexportar.
        public string LevelName { get; set; } = "arena_editor";

        // Nullable, como en el JSON: un nivel a medio construir puede no tener
        // jugador todavía (exportar lo exige, editar no).
        public PlayerData? Player { get; set; }
        public DoorData? Door { get; set; }

        public List<EnemyData> Enemies { get; } = new();
        public List<ObstacleData> Obstacles { get; } = new();
        public List<GearData> Gears { get; } = new();
        public List<SpawnerData> Spawners { get; } = new();
        public List<HealthKitData> HealthKits { get; } = new();
        public List<BarrelData> Barrels { get; } = new();
        public List<HazardData> Hazards { get; } = new();
        public List<PowerUpData> PowerUps { get; } = new();
        public List<ElectricTileData> ElectricTiles { get; } = new();

        public void LoadFrom(LevelData level)
        {
            LevelName = level.LevelName;
            Player = level.Player;
            Door = level.Door;

            Replace(Enemies, level.Enemies);
            Replace(Obstacles, level.Obstacles);
            Replace(Gears, level.Gears);
            Replace(Spawners, level.Spawners);
            Replace(HealthKits, level.HealthKits);
            Replace(Barrels, level.Barrels);
            Replace(Hazards, level.Hazards);
            Replace(PowerUps, level.PowerUps);
            Replace(ElectricTiles, level.ElectricTiles);
        }

        private static void Replace<T>(List<T> target, List<T> source)
        {
            target.Clear();
            target.AddRange(source);
        }

        // El llamante garantiza Player != null (MainForm lo comprueba antes de
        // exportar y avisa si falta): LevelData.Player no es nullable porque
        // LevelLoader.cpp exige la clave "player" para construir la partida.
        public LevelData ToLevelData() => new()
        {
            LevelName = LevelName,
            Player = Player!,
            Obstacles = Obstacles,
            Enemies = Enemies,
            Gears = Gears,
            Spawners = Spawners,
            HealthKits = HealthKits,
            Barrels = Barrels,
            Hazards = Hazards,
            PowerUps = PowerUps,
            ElectricTiles = ElectricTiles,
            Door = Door
        };

        // Orden INVERSO al de dibujado (lo último pintado, arriba del todo, se
        // prueba primero): Player, Enemies, Spawners, Barrels, PowerUps,
        // HealthKits, Gears, Hazards, ElectricTiles, Door, Obstacles.
        public object? FindAt(Point screenPoint)
        {
            if (Player is not null && IsPointNearMarker(screenPoint, WorldToScreen(Player.Spawn)))
                return Player;

            object? marker = FindMarker(Enemies, e => e.Spawn, screenPoint)
                          ?? FindMarker(Spawners, s => s.Position, screenPoint)
                          ?? FindMarker(Barrels, b => b.Position, screenPoint)
                          ?? FindMarker(PowerUps, p => p.Position, screenPoint)
                          ?? FindMarker(HealthKits, h => h.Position, screenPoint)
                          ?? FindMarker(Gears, g => g.Position, screenPoint);
            if (marker is not null) return marker;

            object? plate = FindPlate(Hazards, h => GetBoxScreenRect(h.Position, GetHazardHalfExtents(h)), screenPoint)
                         ?? FindPlate(ElectricTiles, t => GetBoxScreenRect(t.Position, GetElectricTileHalfExtents(t)), screenPoint);
            if (plate is not null) return plate;

            if (Door is not null && GetBoxScreenRect(Door.Position, Door.HalfExtents).Contains(screenPoint))
                return Door;

            for (int i = Obstacles.Count - 1; i >= 0; i--)
                if (IsObstacleHit(Obstacles[i], screenPoint))
                    return Obstacles[i];

            return null;
        }

        // Los dos helpers de abajo recorren de atrás hacia delante por el mismo
        // motivo que FindAt: lo colocado más tarde se dibuja encima, así que
        // gana el hit-test.
        private static object? FindMarker<T>(List<T> items, Func<T, Vector3Data> position, Point point)
        {
            for (int i = items.Count - 1; i >= 0; i--)
                if (IsPointNearMarker(point, WorldToScreen(position(items[i]))))
                    return items[i];
            return null;
        }

        private static object? FindPlate<T>(List<T> items, Func<T, Rectangle> rect, Point point)
        {
            for (int i = items.Count - 1; i >= 0; i--)
                if (rect(items[i]).Contains(point))
                    return items[i];
            return null;
        }

        // true si de verdad se borró algo. Player y Door son singulares (se
        // anulan); el resto sale de su lista.
        public bool Remove(object entity)
        {
            if (ReferenceEquals(entity, Player)) { Player = null; return true; }
            if (ReferenceEquals(entity, Door)) { Door = null; return true; }

            return entity switch
            {
                EnemyData e => Enemies.Remove(e),
                ObstacleData o => Obstacles.Remove(o),
                GearData g => Gears.Remove(g),
                SpawnerData s => Spawners.Remove(s),
                HealthKitData h => HealthKits.Remove(h),
                BarrelData b => Barrels.Remove(b),
                HazardData z => Hazards.Remove(z),
                PowerUpData p => PowerUps.Remove(p),
                ElectricTileData t => ElectricTiles.Remove(t),
                _ => false
            };
        }

        public static void SetPosition(object entity, Vector3Data worldPos)
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
                case PowerUpData powerUp: powerUp.Position = worldPos; break;
                case ElectricTileData tile: tile.Position = worldPos; break;
            }
        }

        // Quita el punto de patrulla más cercano al cursor (dentro del radio de
        // un marcador). true si quitó alguno.
        public static bool RemoveNearestPatrolPoint(EnemyData enemy, Point screenPoint)
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
    }
}
