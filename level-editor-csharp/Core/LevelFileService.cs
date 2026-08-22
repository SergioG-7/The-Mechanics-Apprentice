using System.Text.Json;
using System.Text.Json.Serialization;
using LevelEditor.Models;

namespace LevelEditor.Core
{
    // Carga y guarda niveles en disco como JSON.
    public static class LevelFileService
    {
        private static readonly JsonSerializerOptions SaveOptions = new()
        {
            WriteIndented = true,
            // Omite del JSON las claves con valor null en vez de escribirlas.
            DefaultIgnoreCondition = JsonIgnoreCondition.WhenWritingNull
        };

        public static LevelData Load(string path)
        {
            string json = File.ReadAllText(path);
            LevelData level = JsonSerializer.Deserialize<LevelData>(json)
                ?? throw new InvalidDataException("El archivo no contiene un nivel válido.");

            MigrateLegacyObstacles(level.Obstacles);
            return level;
        }

        public static void Save(LevelData level, string path)
        {
            string json = JsonSerializer.Serialize(level, SaveOptions);
            File.WriteAllText(path, json);
        }

        // Convierte los obstáculos con formato antiguo ("halfExtents") al formato actual.
        private static void MigrateLegacyObstacles(List<ObstacleData> obstacles)
        {
            foreach (ObstacleData obstacle in obstacles)
            {
                if (obstacle.LegacyHalfExtents is Vector3Data legacy)
                {
                    obstacle.Size = new Vector3Data(legacy.X * 2.0f, legacy.Y * 2.0f, legacy.Z * 2.0f);
                    obstacle.LegacyHalfExtents = null;
                }
            }
        }
    }
}
