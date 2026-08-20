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
                // args[0] es el código de idioma que pasa el motor C++ al
                // lanzar este editor desde su botón/atajo (ver
                // Application::LaunchLevelEditor) -- así el editor abre
                // sincronizado con el idioma activo del juego en lugar de
                // arrancar siempre en el último elegido dentro del propio
                // editor. Sin argumento (abierto a mano, doble clic), se
                // mantiene el comportamiento de siempre: el idioma guardado
                // en editor_settings.json. LocalizationManager.SetLanguage ya
                // cae a "es" por su cuenta si el código no es reconocido, así
                // que un argumento inesperado no puede romper el arranque.
                string initialLanguage = args.Length > 0 ? args[0] : EditorSettings.Load().Language;
                LocalizationManager.LoadAll(initialLanguage);
                ApplicationConfiguration.Initialize();
                Application.Run(new MainForm());
        }
    }
}