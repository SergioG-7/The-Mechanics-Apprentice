using System.Text.Json;
using System.Text.Json.Serialization;
using LevelEditor.Models;

namespace LevelEditor.Core
{
    // Serialización de LevelData a/desde disco, incluida la migración de
    // obstáculos en formato legado -- separado de MainForm para que el
    // control de UI (diálogos, MessageBox, listas en memoria) no cargue
    // también con el pipeline de datos (auditoría de arquitectura, 2026-08-19).
    public static class LevelFileService
    {
        private static readonly JsonSerializerOptions SaveOptions = new()
        {
            WriteIndented = true,
            // Si Door es null, se omite la clave "door" del JSON en vez de
            // escribir "door": null -- LevelLoader.cpp distingue "ausente"
            // (nivel sin puerta) de "presente pero null" (que le haría
            // fallar el parseo al intentar leer position/halfExtents).
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

        // Un nivel de antes de la fase de estructuras de entorno trae
        // "halfExtents" en vez de "size" y no declara "type" -- se resuelve
        // una vez al cargar y nunca se vuelve a escribir LegacyHalfExtents
        // (ver ObstacleData).
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
