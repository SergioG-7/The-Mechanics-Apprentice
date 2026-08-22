using LevelEditor.Models;

namespace LevelEditor.Canvas
{
    // Convierte coordenadas entre el mundo del nivel y la pantalla del editor, y detecta clics.
    internal static class CanvasGeometry
    {
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

        // Rectángulo en pantalla de una entidad con posición y tamaño (obstáculo, puerta, hazard...).
        public static Rectangle GetBoxScreenRect(Vector3Data position, Vector3Data halfExtents)
        {
            Point center = WorldToScreen(position);
            int halfWidthPx = Math.Max((int)Math.Round(halfExtents.X * CellSize), MarkerRadius);
            int halfHeightPx = Math.Max((int)Math.Round(halfExtents.Z * CellSize), MarkerRadius);
            return new Rectangle(center.X - halfWidthPx, center.Y - halfHeightPx, halfWidthPx * 2, halfHeightPx * 2);
        }

        public static Vector3Data GetObstacleBoxHalfExtents(ObstacleData obstacle) =>
            new(obstacle.Size.X * 0.5f, obstacle.Size.Y * 0.5f, obstacle.Size.Z * 0.5f);

        public static Vector3Data GetHazardHalfExtents(HazardData hazard) =>
            new(hazard.Size.X * 0.5f, hazard.Size.Y * 0.5f, hazard.Size.Z * 0.5f);

        public static Vector3Data GetElectricTileHalfExtents(ElectricTileData tile) =>
            new(tile.Size.X * 0.5f, tile.Size.Y * 0.5f, tile.Size.Z * 0.5f);

        // Centro y radio en pantalla de un obstáculo circular.
        public static (Point Center, int RadiusPx) GetCylinderScreenCircle(ObstacleData obstacle)
        {
            Point center = WorldToScreen(obstacle.Position);
            int radiusPx = Math.Max((int)Math.Round(obstacle.Radius * CellSize), MarkerRadius);
            return (center, radiusPx);
        }

        // Comprueba si un punto de pantalla cae dentro de un obstáculo.
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

        // Comprueba si un punto de pantalla está lo bastante cerca de un marcador para seleccionarlo.
        public static bool IsPointNearMarker(Point point, Point markerCenter)
        {
            int dx = point.X - markerCenter.X;
            int dy = point.Y - markerCenter.Y;
            return (dx * dx + dy * dy) <= (MarkerRadius * MarkerRadius);
        }
    }
}
