using System.Text.Json;

namespace LevelEditor.Localization
{
    // Estático, no un componente: el texto se pide desde el constructor de
    // MainForm antes de que exista ningún control, igual que en el motor
    // C++ (ver Meta/patrones/localizacion-cjk-unity.md en el vault de
    // Obsidian) -- un ciclo de vida propio solo complicaría el arranque.
    public static class LocalizationManager
    {
        // MainForm se suscribe para reconstruir/reetiquetar todo lo que ya
        // está en pantalla cuando el usuario cambia de idioma en caliente.
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
                    // Fichero mal formado: se queda vacío para ese idioma en
                    // vez de tirar el editor abajo -- GetText hará fallback
                    // a la propia clave para cada texto que faltase.
                }
            }

            Languages[code] = entries;
        }

        public static void SetLanguage(string code)
        {
            CurrentLanguage = Languages.ContainsKey(code) ? code : "es";
            LanguageChanged?.Invoke();
        }

        // Clave no encontrada -> se devuelve la propia clave (nunca cadena
        // vacía: un botón en blanco no se puede diagnosticar, una clave
        // visible sí) y se avisa una sola vez por combinación idioma+clave.
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
