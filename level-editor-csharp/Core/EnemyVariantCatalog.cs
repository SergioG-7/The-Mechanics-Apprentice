using System.Text.Json.Serialization;

namespace LevelEditor.Core
{
    // Stats base de un arquetipo, tal como los define EnemyFactory en el
    // motor C++ (assets/data/enemy_variants.json). "Default" nunca aparece
    // aquí a propósito -- no pasa por EnemyFactory, usa los stats propios
    // del EnemyData tal cual (ver el comentario de EnemyData.Type).
    public sealed class EnemyVariantStatsData
    {
        [JsonPropertyName("maxHP")] public float MaxHP { get; set; }
        [JsonPropertyName("speed")] public float Speed { get; set; }
        [JsonPropertyName("attackDamage")] public float AttackDamage { get; set; }
        [JsonPropertyName("visionRadius")] public float VisionRadius { get; set; }
    }

    // Lectura del mismo enemy_variants.json que consume el motor C++
    // (enlazado, no copiado a mano -- ver LevelEditor.csproj) para que el
    // ComboBox de variante pueda rellenar HP/Velocidad/Rango/Daño con los
    // valores reales del arquetipo en vez de dejarlos todos con el mismo
    // valor por defecto sin importar qué variante se elija.
    public static class EnemyVariantCatalog
    {
        private static Dictionary<string, EnemyVariantStatsData>? _variants;

        public static EnemyVariantStatsData? TryGet(string variantCode)
        {
            EnsureLoaded();
            return _variants!.TryGetValue(variantCode, out EnemyVariantStatsData? stats) ? stats : null;
        }

        private static void EnsureLoaded()
        {
            if (_variants != null) return;
            _variants = new Dictionary<string, EnemyVariantStatsData>();

            string path = Path.Combine(AppContext.BaseDirectory, "Resources", "Data", "enemy_variants.json");
            if (!File.Exists(path)) return;

            try
            {
                string json = File.ReadAllText(path);
                var root = System.Text.Json.JsonSerializer.Deserialize<VariantsRoot>(json);
                if (root?.Variants != null) _variants = root.Variants;
            }
            catch (System.Text.Json.JsonException)
            {
                // Se queda vacío: TryGet devuelve null para cualquier
                // variante y el editor no toca las stats -- mismo criterio
                // de degradar sin reventar que el resto de la carga de
                // datos del editor (ver LocalizationManager.LoadLanguageFile).
            }
        }

        private sealed class VariantsRoot
        {
            [JsonPropertyName("variants")] public Dictionary<string, EnemyVariantStatsData>? Variants { get; set; }
        }
    }
}
