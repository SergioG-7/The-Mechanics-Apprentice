using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    // Generador de oleadas de enemigos.
    public class SpawnerData
    {
        [JsonPropertyName("position")]
        public Vector3Data Position { get; set; } = new();

        [JsonPropertyName("enemyType")]
        public string EnemyType { get; set; } = "Runner";

        [JsonPropertyName("interval")]
        public float Interval { get; set; } = 4.0f;

        [JsonPropertyName("maxEnemies")]
        public int MaxEnemies { get; set; } = 3;

        // Si tiene valores, el spawner sortea el tipo de enemigo por peso en vez de usar siempre EnemyType.
        [JsonPropertyName("weights")]
        public Dictionary<string, int>? Weights { get; set; }

        public bool IsRandom => Weights is { Count: > 0 };
    }
}
