using System;
using System.Windows.Forms;

namespace LevelEditor
{
    internal static class Program
    {
        [STAThread]
        static void Main()
        {
            try
            {
                ApplicationConfiguration.Initialize();
                Application.Run(new MainForm());
            }
            catch (Exception ex)
            {
                // Si el programa explota al arrancar, capturamos el error y lo mostramos en un mensaje
                MessageBox.Show($"Error fatal al iniciar:\n\n{ex.Message}\n\nDetalles:\n{ex.StackTrace}",
                                "Error de Arranque",
                                MessageBoxButtons.OK,
                                MessageBoxIcon.Error);
            }
        }
    }
}