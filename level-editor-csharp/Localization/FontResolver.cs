using System.Drawing.Text;

namespace LevelEditor.Localization
{
    // Resuelve la fuente que hay que aplicar a un control cuyo TEXTO (no el
    // idioma activo) contiene caracteres no-ASCII -- mismo criterio que el
    // motor C++ y Tactical Soccer: una etiqueta ASCII no hereda la fuente
    // CJK solo porque el idioma activo es japonés (ver
    // Meta/patrones/localizacion-cjk-unity.md).
    public static class FontResolver
    {
        // Orden de preferencia: fuentes de Windows con paquete de idioma de
        // Asia Oriental instalado dan mejor legibilidad nativa. Ninguna
        // instalada -> se cae a la fuente embebida del proyecto (paso 2).
        private static readonly string[] CjkFamilyCandidates = { "Yu Gothic UI", "Yu Gothic", "MS Gothic", "Meiryo" };
        private static readonly string EmbeddedFontPath = Path.Combine(AppContext.BaseDirectory, "Resources", "Fonts", "font.ttf");

        private static string? _resolvedOsFamilyName;
        private static bool _osFamilyResolutionAttempted;
        private static PrivateFontCollection? _embeddedFonts;
        private static readonly Dictionary<(float Size, FontStyle Style), Font> Cache = new();

        public static Font ResolveCjkFont(float emSize, FontStyle style = FontStyle.Regular)
        {
            var key = (emSize, style);
            if (Cache.TryGetValue(key, out Font? cached)) return cached;

            Font font = CreateFont(emSize, style);
            Cache[key] = font;
            return font;
        }

        private static Font CreateFont(float emSize, FontStyle style)
        {
            if (!_osFamilyResolutionAttempted)
            {
                _osFamilyResolutionAttempted = true;
                _resolvedOsFamilyName = FindInstalledFamily();
            }

            if (_resolvedOsFamilyName != null)
            {
                return new Font(_resolvedOsFamilyName, emSize, style);
            }

            return LoadEmbeddedFont(emSize, style) ?? new Font(FontFamily.GenericSansSerif, emSize, style);
        }

        private static string? FindInstalledFamily()
        {
            using var installed = new InstalledFontCollection();
            var installedNames = new HashSet<string>(
                installed.Families.Select(f => f.Name), StringComparer.OrdinalIgnoreCase);

            foreach (string candidate in CjkFamilyCandidates)
            {
                if (installedNames.Contains(candidate)) return candidate;
            }
            return null;
        }

        // Red de seguridad para una máquina sin el paquete de idioma de
        // Asia Oriental: el mismo .ttf que ya usa el motor C++
        // (engine-cpp/assets/fonts/font.ttf), enlazado al proyecto del
        // editor -- ver LevelEditor.csproj.
        private static Font? LoadEmbeddedFont(float emSize, FontStyle style)
        {
            if (!File.Exists(EmbeddedFontPath)) return null;
            try
            {
                _embeddedFonts ??= new PrivateFontCollection();
                if (_embeddedFonts.Families.Length == 0)
                {
                    _embeddedFonts.AddFontFile(EmbeddedFontPath);
                }
                return new Font(_embeddedFonts.Families[0], emSize, style);
            }
            catch (Exception)
            {
                return null;
            }
        }
    }
}
