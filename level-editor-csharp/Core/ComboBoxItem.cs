namespace LevelEditor.Core
{
    // Envoltorio para un ComboBox cuyo texto visible depende del idioma
    // activo pero cuyo valor interno tiene que quedarse fijo al serializar
    // -- ver Meta/patrones/localizacion-cjk-unity.md, "Qué NO traducir": un
    // identificador que el motor C++ consume tal cual (EnemyFactory vía el
    // JSON exportado) no es texto de interfaz, aunque aparezca en un combo.
    // WinForms usa ToString() para pintar cada item de un ComboBox sin
    // DisplayMember/ValueMember explícitos.
    public sealed class ComboBoxItem<T>
    {
        public T Value { get; }
        public string DisplayText { get; }

        public ComboBoxItem(T value, string displayText)
        {
            Value = value;
            DisplayText = displayText;
        }

        public override string ToString() => DisplayText;
    }
}
