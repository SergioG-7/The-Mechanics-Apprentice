using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    // Baldosa eléctrica: avisa un momento y luego suelta un único golpe a quien esté encima.
    public class ElectricTileData
    {
        [JsonPropertyName("position")]
        public Vector3Data Position { get; set; } = new();

        [JsonPropertyName("size")]
        public Vector3Data Size { get; set; } = new(2.0f, 0.1f, 2.0f);

        [JsonPropertyName("damage")]
        public float Damage { get; set; } = 20.0f;

        // 0 = solo se arma al pisarla. Mayor que 0 = se arma sola cada tantos segundos.
        [JsonPropertyName("cycleInterval")]
        public float CycleInterval { get; set; }
    }
}
