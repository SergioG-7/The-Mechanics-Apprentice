using System.Text.Json.Serialization;

namespace LevelEditor.Models
{
    // Geometría estática que bloquea el movimiento en el motor C++: "box"
    // (muro/bloque, tamaño variable) o "cylinder" (pilar redondo). Ambas
    // formas comparten esta única clase y viven en la misma lista/JSON
    // ("obstacles") -- LevelLoader.cpp las trata igual porque las dos son un
    // Entity con AABB (el del cilindro, aproximado). Los campos que no
    // aplican al Type activo se serializan igualmente a su valor por
    // defecto; LevelLoader.cpp los ignora sin más, así que no hace falta un
    // conversor JSON polimórfico para un editor de un solo desarrollador.
    public class ObstacleData
    {
        [JsonPropertyName("position")]
        public Vector3Data Position { get; set; } = new();

        [JsonPropertyName("type")]
        public string Type { get; set; } = "box";

        // "box": dimensión completa (no halfExtents) -- LevelLoader.cpp hace
        // halfExtents = size * 0.5. Sin declarar, el motor asume 1,1,1.
        [JsonPropertyName("size")]
        public Vector3Data Size { get; set; } = new(1.0f, 1.0f, 1.0f);

        // "cylinder" únicamente.
        [JsonPropertyName("radius")]
        public float Radius { get; set; } = 0.5f;

        [JsonPropertyName("height")]
        public float Height { get; set; } = 1.0f;

        // Compatibilidad de LECTURA con niveles de antes de esta fase
        // (`sample_level.json`, `test_export.json`): esos ficheros llevan
        // "halfExtents" en vez de "size" y no tienen "type". Nunca se
        // escribe -- MainForm.OnOpenButtonClick migra su valor a Size al
        // abrir y lo deja en null, así que el export nunca vuelve a emitirlo
        // (DefaultIgnoreCondition.WhenWritingNull, ver MainForm.cs).
        [JsonPropertyName("halfExtents")]
        public Vector3Data? LegacyHalfExtents { get; set; }
    }
}
