using System;
using System.Windows.Forms;
using LevelEditor.Core;
using LevelEditor.Localization;

namespace LevelEditor
{
    internal static class Program
    {
        [STAThread]
        static void Main(string[] args)
        {
                // Si el motor C++ lanza el editor, pasa el idioma activo como argumento.
                // Si se abre a mano, se usa el idioma guardado en los ajustes.
                string initialLanguage = args.Length > 0 ? args[0] : EditorSettings.Load().Language;
                LocalizationManager.LoadAll(initialLanguage);
                ApplicationConfiguration.Initialize();
                Application.Run(new MainForm());
        }
    }
}