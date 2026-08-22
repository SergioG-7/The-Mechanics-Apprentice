using System.Text.Json;

namespace LevelEditor.Core
{
    public class EditorSettingsData
    {
        public string Language { get; set; } = "es";
    }

    // Ajustes del propio editor (no del nivel), guardados junto al ejecutable.
    public static class EditorSettings
    {
        private static readonly string SettingsPath = Path.Combine(AppContext.BaseDirectory, "editor_settings.json");

        public static EditorSettingsData Load()
        {
            if (!File.Exists(SettingsPath)) return new EditorSettingsData();

            try
            {
                string json = File.ReadAllText(SettingsPath);
                return JsonSerializer.Deserialize<EditorSettingsData>(json) ?? new EditorSettingsData();
            }
            catch (JsonException)
            {
                return new EditorSettingsData();
            }
        }

        public static void Save(EditorSettingsData data)
        {
            var options = new JsonSerializerOptions { WriteIndented = true };
            File.WriteAllText(SettingsPath, JsonSerializer.Serialize(data, options));
        }
    }
}
