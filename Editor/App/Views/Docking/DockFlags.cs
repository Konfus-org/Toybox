using System;

namespace Toybox.Views.Docking
{
    [Flags]
    public enum DockFlags
    {
        None = 0,
        Left = 1,
        Top = 2,
        Right = 4,
        Bottom = 8,
        All = Left | Top | Right | Bottom
    }
}
