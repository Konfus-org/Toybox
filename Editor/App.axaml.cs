using System.Runtime.InteropServices;
using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using EditorSharp.ViewModels;
using EditorSharp.Views;
using EditorSharp.Views.Windows;

namespace EditorSharp;

public partial class App : Application
{
    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
    }

    public override void OnFrameworkInitializationCompleted()
    {
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            var coreHandle = LaunchEditorCore();
            desktop.MainWindow = new MainWindow
            {
                DataContext = new MainWindowVM(),
            };
        }

        base.OnFrameworkInitializationCompleted();
    }

    [DllImport("EditorCore.dll")]
    public static extern int LaunchEditorCore();
}