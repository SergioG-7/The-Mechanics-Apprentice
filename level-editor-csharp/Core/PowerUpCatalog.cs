using LevelEditor.Localization;

namespace LevelEditor.Core
{
    // Tipos de power-up que reconoce PowerUp::ParseType en el motor C++.
    // Identificadores consumidos tal cual, no texto de interfaz -- lo que se
    // traduce es solo su representación en el ComboBox (ver BuildItems y el
    // mismo criterio en EnemyVariantCatalog).
    public static class PowerUpCatalog
    {
        public static readonly string[] Names = { "Overclock", "Frenzy", "Shield" };

        // Envuelve cada código en un ComboBoxItem cuyo texto visible sale de
        // "powerup_{code}" (powerup_Overclock, powerup_Frenzy...) en el idioma
        // activo, pero cuyo Value se queda en el código en inglés que espera
        // el motor.
        public static ComboBoxItem<string>[] BuildItems() =>
            Names.Select(code => new ComboBoxItem<string>(code, LocalizationManager.GetText($"powerup_{code}"))).ToArray();
    }
}
