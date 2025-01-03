using Avalonia.Controls;
using Tbx.ViewModels;
using SukiUI.Controls;

namespace Tbx.Views;

public partial class MainWindow : SukiWindow
{
    public MainWindow()
    {
        DataContext = new MainWindowVM();
        InitializeComponent();
    }
}