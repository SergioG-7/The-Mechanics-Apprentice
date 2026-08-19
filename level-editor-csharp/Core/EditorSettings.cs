using System.Text.Json;

namespace LevelEditor.Core
{
    public class EditorSettingsData
    {
        public string Language { get; set; } = "es";
    }

    // Ajustes del propio editor (no del nivel que se está editando),
    // persistidos junto al ejecutable. Hoy solo guarda el idioma elegido,
    // pero es el sitio donde crecería cualquier preferencia futura del
    // editor -- igual que SaveManager en el motor C++ separa "datos de
    // partida" de "configuración de la app".
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
