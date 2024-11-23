using Avalonia.Controls;
using Avalonia.Input;
using Avalonia.Platform;
using System;
using System.Threading.Tasks;
namespace Toybox.Views;

public partial class ThreeDViewport : UserControl
{
    public ThreeDViewport()
    {
        DataContext = this;
        InitializeComponent();
    }
}

public class Native3DViewport : NativeControlHost
{
    private Task? _viewportRenderTask;
    private bool _isRendering;

    public Native3DViewport()
    {
        DataContext = this;
    }

    protected override IPlatformHandle CreateNativeControlCore(IPlatformHandle parent)
    {
        Int32 editorCoreHandle = LaunchViewportCore();
        var platformHandle = new PlatformHandle(editorCoreHandle, Name);
        return platformHandle;
    }


    private Int32 LaunchViewportCore()
    {
        Int32 editorCoreHandle = 0;
        bool viewportLaunched = false;

        void CoreUpdateLoop()
        {
            if (!viewportLaunched)
            {
                editorCoreHandle = Core.Interop.LaunchViewport();
                viewportLaunched = true;
                _isRendering = true;
            }

            while (_isRendering)
            {
                Core.Interop.UpdateViewport();
            }

            Core.Interop.CloseViewport();
        }

        _viewportRenderTask = Task.Factory.StartNew(
            action: CoreUpdateLoop,
            creationOptions: TaskCreationOptions.LongRunning);

        while (!viewportLaunched)
        {
            // do nothing... waiting for the viewport to launch
        }

        return editorCoreHandle;
    }

    private void KillViewport()
    {
        _isRendering = false;
    }
}