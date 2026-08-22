namespace LevelEditor.Localization
{
    // Aplica texto traducido a un control, eligiendo la fuente adecuada según el idioma.
    public static class LocalizedControls
    {
        // Traduce y aplica el texto a un control existente, ajustando su fuente si hace falta.
        public static void ApplyText(Control control, string textKey, Font baseFont)
        {
            string text = LocalizationManager.GetText(textKey);
            control.Text = text;
            control.Font = LocalizationManager.ContainsNonAscii(text)
                ? FontResolver.ResolveCjkFont(baseFont.Size, control.Font.Style)
                : baseFont;
        }

        // Crea un Label nuevo ya con el texto traducido.
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
