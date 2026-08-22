using LevelEditor.Localization;

namespace LevelEditor.Core
{
    // Tipos de power-up disponibles.
    public static class PowerUpCatalog
    {
        public static readonly string[] Names = { "Overclock", "Frenzy", "Shield" };

        // Construye los items del ComboBox con el nombre traducido de cada power-up.
        public static ComboBoxItem<string>[] BuildItems() =>
            Names.Select(code => new ComboBoxItem<string>(code, LocalizationManager.GetText($"powerup_{code}"))).ToArray();
    }
}
