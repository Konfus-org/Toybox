using System.Runtime.InteropServices;
namespace Tbx.Core;

public class Interop
{
    [DllImport("../Interop/Interop.dll")]
    public static extern int LaunchViewport();

    [DllImport("../Interop/Interop.dll")]
    public static extern void UpdateViewport();

    [DllImport("../Interop/Interop.dll")]
    public static extern void CloseViewport();
}