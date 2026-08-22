using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    // Geometría estática que bloquea el movimiento: un bloque ("box") o un pilar redondo ("cylinder").
    public class ObstacleData
    {
        [JsonPropertyName("position")]
        public Vector3Data Position { get; set; } = new();

        [JsonPropertyName("type")]
        public string Type { get; set; } = "box";

        // Dimensiones del bloque (solo para type "box").
        [JsonPropertyName("size")]
        public Vector3Data Size { get; set; } = new(1.0f, 1.0f, 1.0f);

        // Solo para type "cylinder".
        [JsonPropertyName("radius")]
        public float Radius { get; set; } = 0.5f;

        [JsonPropertyName("height")]
        public float Height { get; set; } = 1.0f;

        // Solo para leer niveles antiguos que usaban "halfExtents" en vez de "size".
        [JsonPropertyName("halfExtents")]
        public Vector3Data? LegacyHalfExtents { get; set; }
    }
}
