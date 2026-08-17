using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    public class LevelData
    {
        [JsonPropertyName("levelName")]
        public string LevelName { get; set; } = "untitled";

        [JsonPropertyName("player")]
        public PlayerData Player { get; set; } = new();

        [JsonPropertyName("obstacles")]
        public List<ObstacleData> Obstacles { get; set; } = new();

        [JsonPropertyName("enemies")]
        public List<EnemyData> Enemies { get; set; } = new();

        [JsonPropertyName("gears")]
        public List<GearData> Gears { get; set; } = new();

        [JsonPropertyName("spawners")]
        public List<SpawnerData> Spawners { get; set; } = new();

        [JsonPropertyName("healthKits")]
        public List<HealthKitData> HealthKits { get; set; } = new();

        [JsonPropertyName("barrels")]
        public List<BarrelData> Barrels { get; set; } = new();

        // Nullable: un nivel puede no tener puerta todavía. El serializador
        // se configura para OMITIR la clave si es null (ver MainForm.cs),
        // porque LevelLoader.cpp distingue "la clave no existe" de
        // "la clave existe con valor null" -- lo segundo rompería el parseo.
        [JsonPropertyName("door")]
        public DoorData? Door { get; set; }
    }
}
