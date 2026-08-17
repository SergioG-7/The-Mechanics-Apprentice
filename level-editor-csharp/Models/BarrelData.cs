using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    public class BarrelData
    {
        [JsonPropertyName("position")]
        public Vector3Data Position { get; set; } = new();
    }
}
