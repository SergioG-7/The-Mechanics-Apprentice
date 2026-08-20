using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    // Baldosa eléctrica: NO bloquea el paso en el motor C++ (vive en su propia
    // lista "electricTiles", nunca en "obstacles"). A diferencia de un Hazard,
    // no daña por tick: avisa ~2 s y suelta UN golpe a todo lo que siga
    // encima, incluidos los enemigos. Ver ElectricTile.h/.cpp en engine-cpp.
    public class ElectricTileData
    {
        [JsonPropertyName("position")]
        public Vector3Data Position { get; set; } = new();

        [JsonPropertyName("size")]
        public Vector3Data Size { get; set; } = new(2.0f, 0.1f, 2.0f);

        [JsonPropertyName("damage")]
        public float Damage { get; set; } = 20.0f;

        // 0 = solo se arma cuando alguien la pisa. > 0 = además se arma sola
        // cada tantos segundos (baldosa "de ciclo").
        [JsonPropertyName("cycleInterval")]
        public float CycleInterval { get; set; }
    }
}
