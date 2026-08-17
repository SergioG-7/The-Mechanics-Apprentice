using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    // Generador de oleadas: LevelLoader.cpp lo lee y crea un Spawner en
    // tiempo de ejecución que instancia enemigos vía EnemyFactory (ver
    // engine-cpp/src/Entities/EnemyFactory.h) usando EnemyType como nombre
    // de arquetipo -- no admite "Default", solo variantes reales del JSON
    // de assets/data/enemy_variants.json.
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
    }
}
