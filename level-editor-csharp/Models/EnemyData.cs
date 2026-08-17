using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    public class EnemyData
    {
        [JsonPropertyName("spawn")]
        public Vector3Data Spawn { get; set; } = new();

        // Arquetipo data-driven ("Default", "Tank", "Runner", ...) que
        // EnemyFactory resuelve contra assets/data/enemy_variants.json.
        // "Default" es un caso especial que LevelLoader.cpp no busca en el
        // factory: construye el Enemy directamente con los stats de abajo,
        // igual que hacía antes de que existieran las variantes.
        [JsonPropertyName("type")]
        public string Type { get; set; } = "Default";

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
