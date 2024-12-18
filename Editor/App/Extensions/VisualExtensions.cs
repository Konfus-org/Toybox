using Avalonia;

namespace Toybox.Extensions
{
    public static class VisualExtensions
    {
        public static Rect GetBoundsOf(this Visual self, Visual visual)
        {
            Point topLeft = visual.TranslatePoint(new Point(0, 0), self)!.Value;
            Point bottomRight = visual.TranslatePoint(new Point(visual.Bounds.Width, visual.Bounds.Height), self)!.Value;
            return new Rect(topLeft, bottomRight);
        }
    }
}
