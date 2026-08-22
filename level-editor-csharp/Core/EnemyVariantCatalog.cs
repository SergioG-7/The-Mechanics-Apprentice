using System.Text.Json.Serialization;
using LevelEditor.Localization;

namespace LevelEditor.Core
{
    // Stats base de un arquetipo de enemigo.
    public sealed class EnemyVariantStatsData
    {
        [JsonPropertyName("maxHP")] public float MaxHP { get; set; }
        [JsonPropertyName("speed")] public float Speed { get; set; }
        [JsonPropertyName("attackDamage")] public float AttackDamage { get; set; }
        [JsonPropertyName("visionRadius")] public float VisionRadius { get; set; }
    }

    // Lee las stats de cada arquetipo de enemigo desde el JSON de datos del motor.
    public static class EnemyVariantCatalog
    {
        // Arquetipos disponibles para elegir en el ComboBox.
        public static readonly string[] Names =
            { "Default", "Tank", "Runner", "Spitter", "Kamikaze", "Shielder", "Buffer", "Trapper" };

        // Construye los items del ComboBox con el nombre traducido de cada arquetipo.
        public static ComboBoxItem<string>[] BuildItems(IEnumerable<string> codes) =>
            codes.Select(code => new ComboBoxItem<string>(code, LocalizationManager.GetText($"variant_{code}"))).ToArray();

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
                // Si el JSON está mal, se queda vacío en vez de romper el editor.
            }
        }

        private sealed class VariantsRoot
        {
            [JsonPropertyName("variants")] public Dictionary<string, EnemyVariantStatsData>? Variants { get; set; }
        }
    }
}
