using LevelEditor.Models;

namespace LevelEditor.Canvas
{
    // Conversión de coordenadas mundo↔pantalla e hit-testing geométrico del
    // lienzo 2D del editor -- matemática pura sin estado, separada de
    // MainForm para que la UI no cargue también con estos cálculos
    // (auditoría de arquitectura, 2026-08-19). MainForm la usa con
    // "using static", así que las llamadas siguen escribiéndose sin
    // cualificar (ScreenToWorld(...), no CanvasGeometry.ScreenToWorld(...)).
    internal static class CanvasGeometry
    {
        // 20 px por unidad de mundo sobre un lienzo de 700: cubre de -17.5 a
        // +17.5, es decir el perímetro entero de la arena ampliada (muros en
        // ±16, ver assets/data/*.json y Application::DrawGroundGrid). Con los
        // 30/600 anteriores el lienzo solo llegaba a ±10 y ni siquiera se
        // podían ver los muros, mucho menos colocar nada más allá de ellos.
        public const int CellSize = 20;
        public const int CanvasSize = 700;
        public static readonly Point CanvasCenter = new(CanvasSize / 2, CanvasSize / 2);
        public const int MarkerRadius = 8;

        public static Vector3Data ScreenToWorld(Point screenPoint)
        {
            float worldX = (screenPoint.X - CanvasCenter.X) / (float)CellSize;
            float worldZ = (screenPoint.Y - CanvasCenter.Y) / (float)CellSize;
            return new Vector3Data(worldX, 0.0f, worldZ);
        }

        public static Point WorldToScreen(Vector3Data world)
        {
            int screenX = CanvasCenter.X + (int)Math.Round(world.X * CellSize);
            int screenY = CanvasCenter.Y + (int)Math.Round(world.Z * CellSize);
            return new Point(screenX, screenY);
        }

        // Rect en pantalla de cualquier entidad con posición + halfExtents
        // (Obstacle-box, Door y Hazard comparten esta forma). Usado tanto
        // para dibujar como para el hit-test de selección.
        public static Rectangle GetBoxScreenRect(Vector3Data position, Vector3Data halfExtents)
        {
            Point center = WorldToScreen(position);
            int halfWidthPx = Math.Max((int)Math.Round(halfExtents.X * CellSize), MarkerRadius);
            int halfHeightPx = Math.Max((int)Math.Round(halfExtents.Z * CellSize), MarkerRadius);
            return new Rectangle(center.X - halfWidthPx, center.Y - halfHeightPx, halfWidthPx * 2, halfHeightPx * 2);
        }

        // ObstacleData guarda Size (dimensión completa, formato nuevo) para
        // el tipo "box" -- este helper es el único sitio que lo convierte a
        // halfExtents para reusar GetBoxScreenRect.
        public static Vector3Data GetObstacleBoxHalfExtents(ObstacleData obstacle) =>
            new(obstacle.Size.X * 0.5f, obstacle.Size.Y * 0.5f, obstacle.Size.Z * 0.5f);

        public static Vector3Data GetHazardHalfExtents(HazardData hazard) =>
            new(hazard.Size.X * 0.5f, hazard.Size.Y * 0.5f, hazard.Size.Z * 0.5f);

        public static Vector3Data GetElectricTileHalfExtents(ElectricTileData tile) =>
            new(tile.Size.X * 0.5f, tile.Size.Y * 0.5f, tile.Size.Z * 0.5f);

        // Círculo en pantalla de un Obstacle tipo "cylinder": centro + radio en píxeles.
        public static (Point Center, int RadiusPx) GetCylinderScreenCircle(ObstacleData obstacle)
        {
            Point center = WorldToScreen(obstacle.Position);
            int radiusPx = Math.Max((int)Math.Round(obstacle.Radius * CellSize), MarkerRadius);
            return (center, radiusPx);
        }

        public static bool IsObstacleHit(ObstacleData obstacle, Point screenPoint)
        {
            if (obstacle.Type == "cylinder")
            {
                var (center, radiusPx) = GetCylinderScreenCircle(obstacle);
                int dx = screenPoint.X - center.X;
                int dy = screenPoint.Y - center.Y;
                return (dx * dx + dy * dy) <= (radiusPx * radiusPx);
            }

            return GetBoxScreenRect(obstacle.Position, GetObstacleBoxHalfExtents(obstacle)).Contains(screenPoint);
        }

        public static bool IsPointNearMarker(Point point, Point markerCenter)
        {
            int dx = point.X - markerCenter.X;
            int dy = point.Y - markerCenter.Y;
            return (dx * dx + dy * dy) <= (MarkerRadius * MarkerRadius);
        }
    }
}
