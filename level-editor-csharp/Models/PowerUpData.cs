using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    // Pickup de efecto temporal. Type es un identificador que el motor C++
    // consume tal cual (PowerUp::ParseType en engine-cpp), no texto de
    // interfaz: se traduce su representación en el ComboBox, nunca el valor
    // serializado -- mismo criterio que EnemyData.Type.
    public class PowerUpData
    {
        [JsonPropertyName("position")]
        public Vector3Data Position { get; set; } = new();

        [JsonPropertyName("type")]
        public string Type { get; set; } = "Overclock";
    }
}
