using Avalonia;
using Avalonia.Controls.ApplicationLifetimes;
using Avalonia.Markup.Xaml;
using Avalonia.Styling;
using SukiUI;
using Tbx.ViewModels;
using Tbx.Views;

namespace Tbx;

public partial class App : Application
{
    public override void Initialize()
    {
        AvaloniaXamlLoader.Load(this);
    }

    public override void OnFrameworkInitializationCompleted()
    {
        // Setup main window
        if (ApplicationLifetime is IClassicDesktopStyleApplicationLifetime desktop)
        {
            var mainWindow = new MainWindow
            {
                DataContext = new MainWindowVM(),
            };
            desktop.MainWindow = mainWindow;
        }

        // Default to dark theme
        SukiTheme.GetInstance().ChangeBaseTheme(ThemeVariant.Dark);

        base.OnFrameworkInitializationCompleted();
    }

}