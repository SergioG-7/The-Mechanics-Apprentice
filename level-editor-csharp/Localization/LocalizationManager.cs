using System.Text.Json;

namespace LevelEditor.Localization
{
    // Carga y sirve los textos traducidos del editor.
    public static class LocalizationManager
    {
        // Se dispara cuando cambia el idioma, para que la UI se reetiquete.
        public static event Action? LanguageChanged;

        private static readonly string[] SupportedLanguages = { "es", "en", "jp" };
        private static readonly Dictionary<string, Dictionary<string, string>> Languages = new();
        private static readonly HashSet<string> MissingKeysWarned = new();

        public static string CurrentLanguage { get; private set; } = "es";

        public static void LoadAll(string initialLanguage)
        {
            Languages.Clear();
            foreach (string code in SupportedLanguages)
            {
                LoadLanguageFile(code);
            }
            SetLanguage(initialLanguage);
        }

        private static void LoadLanguageFile(string code)
        {
            string path = Path.Combine(AppContext.BaseDirectory, "Resources", "Localization", $"{code}.json");
            Dictionary<string, string> entries = new();

            if (File.Exists(path))
            {
                try
                {
                    string json = File.ReadAllText(path);
                    entries = JsonSerializer.Deserialize<Dictionary<string, string>>(json) ?? entries;
                }
                catch (JsonException)
                {
                    // Si el JSON está mal, ese idioma se queda sin textos en vez de romper el editor.
                }
            }

            Languages[code] = entries;
        }

        public static void SetLanguage(string code)
        {
            CurrentLanguage = Languages.ContainsKey(code) ? code : "es";
            LanguageChanged?.Invoke();
        }

        // Devuelve el texto traducido de una clave. Si no existe, devuelve la propia clave.
        public static string GetText(string key)
        {
            if (Languages.TryGetValue(CurrentLanguage, out var entries) && entries.TryGetValue(key, out var value))
            {
                return value;
            }

            if (MissingKeysWarned.Add($"{CurrentLanguage}:{key}"))
            {
                Console.Error.WriteLine($"LocalizationManager: clave '{key}' no encontrada en '{CurrentLanguage}'");
            }
            return key;
        }

        public static string GetText(string key, params object[] args) => string.Format(GetText(key), args);

        public static bool ContainsNonAscii(string text)
        {
            foreach (char c in text)
            {
                if (c > 127) return true;
            }
            return false;
        }
    }
}
