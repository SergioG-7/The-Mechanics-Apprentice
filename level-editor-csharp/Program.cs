using System;
using System.Windows.Forms;
using LevelEditor.Core;
using LevelEditor.Localization;

namespace LevelEditor
{
    internal static class Program
    {
        [STAThread]
        static void Main()
        {
                LocalizationManager.LoadAll(EditorSettings.Load().Language);
                ApplicationConfiguration.Initialize();
                Application.Run(new MainForm());
        }
    }
}