using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    public class PlayerData
    {
        [JsonPropertyName("spawn")]
        public Vector3Data Spawn { get; set; } = new();

        [JsonPropertyName("maxHP")]
        public float MaxHP { get; set; } = 100.0f;

        [JsonPropertyName("speed")]
        public float Speed { get; set; } = 4.0f;

        [JsonPropertyName("attackDamage")]
        public float AttackDamage { get; set; } = 50.0f;
    }
}
