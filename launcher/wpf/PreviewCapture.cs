using System.IO;
using System.Text.Json;
using System.Windows;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using System.Windows.Threading;

namespace DriveClub.Launcher;

public partial class MainWindow
{
    private async Task CaptureStartupFrames(string directory, Storyboard loading)
    {
        string folder = Path.Combine(directory, "startup-frames"); Directory.CreateDirectory(folder);
        // Chromium stretches durations <= 10 ms to 100 ms. 11 ms is its fastest
        // unmodified animated-image interval; sample the native timeline at that rate.
        const int frameCount = 228, frameMilliseconds = 11;
        int width = (int)Math.Min(1280, Root.ActualWidth);
        int height = (int)Math.Round(width * Root.ActualHeight / Root.ActualWidth);
        var durations = new List<int>();
        spinnerTimer?.Stop(); loading.Pause(this);
        // Sample the actual storyboard at fixed times, independent of rendering/encoding cost.
        for (int i = 0; i < frameCount; i++)
        {
            double time = i * frameMilliseconds / 1000d;
            loading.SeekAlignedToLastTick(this, TimeSpan.FromSeconds(time), TimeSeekOrigin.BeginTime);
            if (spinnerFrames.Count > 0) Spinner.Source = spinnerFrames[(int)(time * 30) % spinnerFrames.Count];
            await Dispatcher.InvokeAsync(() => { }, DispatcherPriority.Render);
            var drawing = new DrawingVisual();
            var brush = new VisualBrush(Root) { ViewboxUnits = BrushMappingMode.Absolute, Viewbox = new Rect(0, 0, Root.ActualWidth, Root.ActualHeight), Stretch = Stretch.Fill };
            using (var context = drawing.RenderOpen()) context.DrawRectangle(brush, null, new Rect(0, 0, width, height));
            var frame = new RenderTargetBitmap(width, height, 96, 96, PixelFormats.Pbgra32);
            frame.Render(drawing); frame.Freeze();
            var encoder = new PngBitmapEncoder(); encoder.Frames.Add(BitmapFrame.Create(frame));
            using var file = File.Create(Path.Combine(folder, $"{i:0000}.png")); encoder.Save(file);
            durations.Add(frameMilliseconds);
        }
        loading.Remove(this);
        await File.WriteAllTextAsync(Path.Combine(folder, "timing.json"), JsonSerializer.Serialize(durations));
    }
}
