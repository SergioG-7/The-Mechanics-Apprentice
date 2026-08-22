using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    // Coordenadas 3D simples, iguales al Vector3 del motor C++.
    public class Vector3Data
    {
        [JsonPropertyName("x")]
        public float X { get; set; }

        [JsonPropertyName("y")]
        public float Y { get; set; }

        [JsonPropertyName("z")]
        public float Z { get; set; }

        public Vector3Data() { }

        public Vector3Data(float x, float y, float z)
        {
            X = x;
            Y = y;
            Z = z;
        }
    }
}
