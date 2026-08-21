namespace LevelEditor.Localization
{
    // Creación y reetiquetado de controles con texto localizado, resolviendo
    // la fuente por CONTENIDO y no por idioma activo (ver
    // Meta/patrones/localizacion-cjk-unity.md): un texto que vuelve a ser
    // ASCII en el idioma nuevo recupera la fuente base explícitamente, en vez
    // de quedarse con la CJK del idioma anterior puesta.
    //
    // Estático y con la fuente base por parámetro porque lo comparten dos
    // sitios que no se conocen entre sí: MainForm (sus controles fijos) y
    // PropertyPanelBuilder (los Label que crea sobre la marcha).
    public static class LocalizedControls
    {
        public static void ApplyText(Control control, string textKey, Font baseFont)
        {
            string text = LocalizationManager.GetText(textKey);
            control.Text = text;
            control.Font = LocalizationManager.ContainsNonAscii(text)
                ? FontResolver.ResolveCjkFont(baseFont.Size, control.Font.Style)
                : baseFont;
        }

        // Para los Label que se crean ya con su texto, sin pasar por un
        // control existente.
        public static Label CreateLabel(string textKey, Point location, Font baseFont)
        {
            string text = LocalizationManager.GetText(textKey);
            var label = new Label { Location = location, AutoSize = true, Text = text };
            if (LocalizationManager.ContainsNonAscii(text))
            {
                label.Font = FontResolver.ResolveCjkFont(baseFont.Size);
            }
            return label;
        }
    }
}
