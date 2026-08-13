using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    public class EnemyData
    {
        [JsonPropertyName("spawn")]
        public Vector3Data Spawn { get; set; } = new();

        [JsonPropertyName("maxHP")]
        public float MaxHP { get; set; }

        [JsonPropertyName("visionRadius")]
        public float VisionRadius { get; set; }

        [JsonPropertyName("speed")]
        public float Speed { get; set; } = 2.5f;

        [JsonPropertyName("attackDamage")]
        public float AttackDamage { get; set; } = 10.0f;

        [JsonPropertyName("patrolRoute")]
        public List<Vector3Data> PatrolRoute { get; set; } = new();
    }
}
