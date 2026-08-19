using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    // Trampa de suelo: NO bloquea el paso en el motor C++ (vive en su propia
    // lista "hazards", nunca en "obstacles") pero daña por tick a quien se
    // quede dentro de sus límites. Ver Hazard.h/.cpp en engine-cpp.
    public class HazardData
    {
        [JsonPropertyName("position")]
        public Vector3Data Position { get; set; } = new();

        [JsonPropertyName("size")]
        public Vector3Data Size { get; set; } = new(2.0f, 0.1f, 2.0f);

        [JsonPropertyName("damagePerTick")]
        public float DamagePerTick { get; set; } = 10.0f;
    }
}
