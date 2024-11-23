using System.Runtime.InteropServices;
namespace Toybox.Core;

public class Interop
{
    [DllImport("../Interop/Interop.dll")]
    public static extern int LaunchViewport();

    [DllImport("../Interop/Interop.dll")]
    public static extern void UpdateViewport();

    [DllImport("../Interop/Interop.dll")]
    public static extern void CloseViewport();
}