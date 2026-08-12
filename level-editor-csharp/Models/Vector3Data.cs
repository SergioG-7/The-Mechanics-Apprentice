using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    // Mapea 1:1 con el struct Vector3 de raylib/C++, tal como lo espera
    // LevelLoader.cpp: tres floats en minúsculas ("x", "y", "z").
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
