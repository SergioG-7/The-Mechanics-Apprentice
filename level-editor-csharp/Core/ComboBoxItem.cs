namespace LevelEditor.Core
{
    // Un item de ComboBox con un valor interno fijo y un texto visible que puede traducirse.
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
