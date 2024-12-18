using Avalonia.Controls;
using Toybox.ViewModels;
using SukiUI.Controls;

namespace Toybox.Views;

public partial class MainWindow : SukiWindow
{
    public MainWindow()
    {
        DataContext = new MainWindowVM();
        InitializeComponent();
    }
}