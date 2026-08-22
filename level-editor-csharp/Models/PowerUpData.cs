using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    // Pickup de efecto temporal para el jugador.
    public class PowerUpData
    {
        [JsonPropertyName("position")]
        public Vector3Data Position { get; set; } = new();

        [JsonPropertyName("type")]
        public string Type { get; set; } = "Overclock";
    }
}
