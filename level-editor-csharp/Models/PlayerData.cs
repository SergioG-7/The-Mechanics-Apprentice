using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    public class PlayerData
    {
        [JsonPropertyName("spawn")]
        public Vector3Data Spawn { get; set; } = new();

        // Nota: sigue siendo float, no int. El pedido decía "MaxHP (int)"
        // pero ya existía como float desde la Fase 2 y así coincide con
        // Actor::m_hp en C++ (también float). Cambiarlo a int no rompería
        // el JSON (los números no distinguen), pero sí introduciría un tipo
        // distinto al del motor sin necesidad real -- avísame si de verdad
        // lo quieres como int.
        [JsonPropertyName("maxHP")]
        public float MaxHP { get; set; } = 100.0f;

        [JsonPropertyName("speed")]
        public float Speed { get; set; } = 4.0f;

        [JsonPropertyName("attackDamage")]
        public int AttackDamage { get; set; } = 50;
    }
}
