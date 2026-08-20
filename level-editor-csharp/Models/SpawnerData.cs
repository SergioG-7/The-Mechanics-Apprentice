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

        // Random Spawner: null (clave omitida) = spawner clásico, siempre
        // EnemyType. Con entradas, el motor sortea arquetipo en cada spawn
        // proporcionalmente al peso (ver Spawner::PickEnemyType) y EnemyType
        // pasa a ser solo el fallback si todos los pesos fueran 0.
        //
        // Nullable a propósito: el serializador omite las claves nulas (ver
        // LevelFileService.SaveOptions), así que un spawner normal sigue
        // exportándose exactamente igual que antes de existir esta mecánica.
        [JsonPropertyName("weights")]
        public Dictionary<string, int>? Weights { get; set; }

        public bool IsRandom => Weights is { Count: > 0 };
    }
}
