using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    public class ObstacleData
    {
        [JsonPropertyName("position")]
        public Vector3Data Position { get; set; } = new();

        [JsonPropertyName("halfExtents")]
        public Vector3Data HalfExtents { get; set; } = new();
    }
}
