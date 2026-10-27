using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Net.Http;
using System.Runtime.InteropServices;
using System.Text.Json;
using System.Windows;
using System.Windows.Automation;
using System.Windows.Controls;
using System.Windows.Controls.Primitives;
using System.Windows.Data;
using System.Windows.Interop;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Media.Imaging;
using System.Windows.Threading;
using Microsoft.Win32;
using Path = System.IO.Path;

namespace DriveClub.Launcher;
public partial class MainWindow : Window
{
    private readonly Backend backend = new();
    private LauncherState state = new();
    private readonly Dictionary<int, FrameworkElement> pages = [];
    private readonly Dictionary<string, BitmapSource> images = [];
    private readonly List<Button> navButtons = [];
    private readonly string[] names = ["Play", "Display", "Graphics", "General", "Driving", "Patches", "Benchmark", "Mods"];
    private readonly string[] hints = ["Your game, ready to drive.", "Frame rate, resolution, anti-aliasing and HDR.", "Scene detail, reflections and camera effects.", "Player profile, content, launcher behavior and advanced options.", "Driving assists, field of view and steering wheel forces.", "Compatibility workarounds and optional community patches.", "A repeatable event driven by the game's AI.", "Manage local replacement files."];
    // Home is shell-only; the remaining pages follow the native 0.3.1 tabs.
    private readonly int[] sourcePages = [-1, 0, 1, 2, 3, 5, 4, -2];
    private readonly string[] backgroundPaths = ["backgrounds/page-0.png", "backgrounds/page-2.png", "backgrounds/detail.png", "backgrounds/page-4.png", "backgrounds/driving.png", "backgrounds/patches.png", "backgrounds/page-5.png", "backgrounds/mods.png"];
    private int drivingSubpage;
    private bool foldersExpanded;
    private bool animateFoldersOpening;
    private bool foldersAnimating;
    private Action? arrangeHome;
    private readonly string root = AppContext.BaseDirectory;
    private int currentPage, revision;
    private int navigationSelection = -1;
    private bool suppress, busy, allowClose;
    private Window? launchSplash;
    private Panel? busyOverlayParent;
    private Button? switchToGame;
    private bool mayCloseSplash;
    private int launchedGameId;
    private LaunchSession? launchSession;
    private string baseStatus = "Loading your settings...";
    private readonly bool preview = Environment.GetCommandLineArgs().Contains("--preview");
    private readonly bool capture = Environment.GetCommandLineArgs().Contains("--capture");
    private readonly bool perf = Environment.GetCommandLineArgs().Contains("--perf");
    private DispatcherTimer? spinnerTimer;
    private int spinnerIndex;
    private readonly List<BitmapSource> spinnerFrames = [];
    private readonly List<double> renderIntervals = [];
    private TimeSpan lastRender;
    private int refreshRate = 240;
    private string? releaseUrl;
    private string baseStatusColour = "#A0A0AA";
    private Task CheckForUpdates() => CheckNativeUpdates();

    [DllImport("dwmapi.dll")] private static extern int DwmSetWindowAttribute(IntPtr hwnd, int attribute, ref int value, int size);
    [DllImport("user32.dll")] private static extern IntPtr GetDC(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern int ReleaseDC(IntPtr hwnd, IntPtr dc);
    [DllImport("gdi32.dll")] private static extern int GetDeviceCaps(IntPtr dc, int index);

    [StructLayout(LayoutKind.Sequential)] private struct NativePoint { public int X, Y; }
    [StructLayout(LayoutKind.Sequential)] private struct NativeRect { public int Left, Top, Right, Bottom; }
    [StructLayout(LayoutKind.Sequential)] private struct MonitorInfo { public int Size; public NativeRect Monitor, Work; public uint Flags; }
    [StructLayout(LayoutKind.Sequential)] private struct MinMaxInfo { public NativePoint Reserved, MaxSize, MaxPosition, MinTrackSize, MaxTrackSize; }
    [DllImport("user32.dll")] private static extern IntPtr MonitorFromWindow(IntPtr hwnd, uint flags);
    [DllImport("user32.dll", CharSet = CharSet.Auto)] private static extern bool GetMonitorInfo(IntPtr monitor, ref MonitorInfo info);
    [DllImport("user32.dll")] private static extern bool GetWindowRect(IntPtr hwnd, out NativeRect rect);
    private IntPtr WindowHook(IntPtr hwnd, int message, IntPtr wparam, IntPtr lparam, ref bool handled)
    {
        if (message != 0x0024) return IntPtr.Zero;
        var info = new MonitorInfo { Size = Marshal.SizeOf<MonitorInfo>() };
        if (!GetMonitorInfo(MonitorFromWindow(hwnd, 2), ref info)) return IntPtr.Zero;
        var limits = Marshal.PtrToStructure<MinMaxInfo>(lparam);
        limits.MaxPosition = new() { X = info.Work.Left - info.Monitor.Left, Y = info.Work.Top - info.Monitor.Top };
        limits.MaxSize = new() { X = info.Work.Right - info.Work.Left, Y = info.Work.Bottom - info.Work.Top };
        var dpi = VisualTreeHelper.GetDpi(this);
        limits.MinTrackSize = new() { X = (int)Math.Ceiling(MinWidth * dpi.DpiScaleX), Y = (int)Math.Ceiling(MinHeight * dpi.DpiScaleY) };
        Marshal.StructureToPtr(limits, lparam, false); handled = true; return IntPtr.Zero;
    }
    public MainWindow()
    {
        LoadFonts();
        InitializeComponent();
        InitializeParity();
        if (preview) Title = "DRIVECLUB Launcher - Preview";
        Status.MouseLeftButtonUp += (_, _) =>
        {
            if (releaseUrl == null || Status.Text != baseStatus) return;
            try { Process.Start(new ProcessStartInfo(releaseUrl) { UseShellExecute = true }); }
            catch (Exception ex) when (ex is Win32Exception or InvalidOperationException) { SetStatus("Could not open the release page in your browser.", true); }
        };
        StateChanged += (_, _) => MaximizeGlyph.Glyph = WindowState == WindowState.Maximized ? "Restore" : "Maximize";
        TitleLogo.Source = Image("brand/logo-red.png");
        LoadOptionBrushes();
        BusyArt.Source = Image("backgrounds/page-0.png", true);
        SwipeOne.Source = SwipeTwo.Source = Image("transitions/swipe.png");
        Stamp.Source = Image("transitions/stamp.png");
        for (int i = 0; i < 29; ++i) if (Image($"spinner/{i:00}.png") is { } frame) spinnerFrames.Add(frame);
        foreach (string background in backgroundPaths) Image(background, true);
        BuildNavigation();
        SourceInitialized += (_, _) =>
        {
            var handle = new WindowInteropHelper(this).Handle;
            HwndSource.FromHwnd(handle)?.AddHook(WindowHook);
            int square = 1; DwmSetWindowAttribute(handle, 33, ref square, sizeof(int));
            int dark = 1; DwmSetWindowAttribute(handle, 20, ref dark, sizeof(int));
            var dc = GetDC(handle); int hz = GetDeviceCaps(dc, 116); ReleaseDC(handle, dc);
            refreshRate = hz > 1 ? hz : 240;
        };
        Closing += (_, e) => { if (busy && !allowClose) { e.Cancel = true; SetStatus("Please wait for the current operation to finish."); } };
        Loaded += async (_, _) =>
        {
            try
            {
                if (Environment.GetCommandLineArgs().Contains("--compact")) { Width = MinWidth; Height = MinHeight; }
                if (Environment.GetCommandLineArgs().Contains("--maximized")) WindowState = WindowState.Maximized;
                Adopt(await backend.Call("state"));
                OperationStatus(state.FoldersOk ? "" : "Choose the game folders on Play.");
                if (Environment.GetCommandLineArgs().Contains("--mod-manager")) ShowPage(ModsPage, false);
                if (capture) await CapturePages();
                else if (perf) await MeasureAnimations();
                else if (!preview && Environment.GetCommandLineArgs().Contains("--benchmark")) { ShowPage(6, false); await Launch(true); }
                else if (!preview && (Environment.GetCommandLineArgs().Contains("--play") || (File.Exists(Path.Combine(root, "launcher.ini")) && File.ReadLines(Path.Combine(root, "launcher.ini")).Any(x => x.Trim() == "auto_play=1")))) await Launch(false);
                if (!preview && !capture && !perf && !busy) _ = CheckForUpdates();
            }
            catch (Exception ex) { SetStatus(ex.Message, true); }
        };
    }
    private void LoadFonts()
    {
        string directory = Path.Combine(root, "assets", "fonts");
        if (!Directory.Exists(directory)) return;
        var uri = new Uri(directory + Path.DirectorySeparatorChar);
        Application.Current.Resources["BodyFont"] = new FontFamily(uri, "./#001Futura");
        Application.Current.Resources["HeadingFont"] = new FontFamily(uri, "./#004Russian");
    }
    private BitmapSource? Image(string relative, bool monochrome = false)
    {
        string key = relative + (monochrome ? ":mono" : "");
        if (images.TryGetValue(key, out var cached)) return cached;
        string file = Path.Combine(root, "assets", relative.Replace('/', Path.DirectorySeparatorChar));
        if (!File.Exists(file)) return null;
        var bitmap = new BitmapImage(); bitmap.BeginInit(); bitmap.CacheOption = BitmapCacheOption.OnLoad;
        bitmap.UriSource = new Uri(file); bitmap.EndInit(); bitmap.Freeze();
        BitmapSource result = bitmap;
        // Runtime conversion happens once per art. Original files remain unchanged.
        if (monochrome) { var gray = new FormatConvertedBitmap(bitmap, PixelFormats.Gray8, null, 0); gray.Freeze(); result = gray; }
        images[key] = result; return result;
    }
    private TextBlock Text(string value, double size = 14, bool heading = false, string color = "#F2F2F4") => new()
    { Text = value, FontSize = size, FontFamily = (FontFamily)Application.Current.FindResource(heading ? "HeadingFont" : "BodyFont"), Foreground = Brush(color), TextWrapping = TextWrapping.Wrap };
    private static SolidColorBrush Brush(string color) { var b = (SolidColorBrush)new BrushConverter().ConvertFromString(color)!; b.Freeze(); return b; }
    private void SetStatus(string text, bool error = false)
    {
        baseStatus = text; baseStatusColour = error ? "#FF9A9F" : "#A0A0AA";
        releaseUrl = null; RestoreStatus();
    }
    private void RestoreStatus()
    {
        Status.Text = baseStatus; Status.Foreground = Brush(baseStatusColour);
        Status.Cursor = releaseUrl == null ? Cursors.Arrow : Cursors.Hand;
        Status.ToolTip = releaseUrl == null ? null : "Opens the release page. Use the Update button to download and install it.";
    }
    private void ShowStatusHint(string text)
    {
        Status.Text = text; Status.Foreground = Brush("#A0A0AA");
        Status.Cursor = Cursors.Arrow; Status.ToolTip = null;
    }
    private void ShowUpdateNotice(string tag)
    {
        SetStatus($"Version {tag} is available. Use Update to install it, or click here for release details.");
        releaseUrl = "https://github.com/OverkillLabs3/driveclub-recomp/releases/tag/" + Uri.EscapeDataString(tag);
        baseStatusColour = "#5AC878"; RestoreStatus();
    }
    private void DiscordClick(object sender, RoutedEventArgs e)
    {
        var result = CommunityLink.Open();
        if (result.NeedsNotice) new CommunityLinkDialog(result.Message) { Owner = this }.ShowDialog();
        else SetStatus("Opening Discord in your browser...");
    }
    private void CopyDiscordClick(object sender, RoutedEventArgs e)
    {
        bool copied = CommunityLink.TryCopy();
        if (copied) SetStatus("Discord link copied: " + CommunityLink.Url);
        else new CommunityLinkDialog("Could not copy the link. Select it below and press Ctrl+C, or try Copy link again.") { Owner = this }.ShowDialog();
    }
    private void LoadOptionBrushes()
    {
        Application.Current.Resources["OptionActive"] = FindResource("Accent");
        foreach (var pair in new[] { ("OptionInactive", "controls/option-inactive.png") })
        {
            var bitmap = Image(pair.Item2);
            if (bitmap is null) continue;
            // The first 192 source pixels fade to transparent black. Use only
            // the opaque portion; grain is rendered after scaling in the template.
            var brush = new ImageBrush(bitmap) { Stretch = Stretch.Fill, Opacity = .72, Viewbox = new Rect(192, 8, 476, 62), ViewboxUnits = BrushMappingMode.Absolute }; brush.Freeze();
            Application.Current.Resources[pair.Item1] = brush;
        }
    }
    private void BuildNavigation()
    {
        for (int i = 0; i < names.Length; ++i)
        {
            int page = i;
            var label = Text(names[i], 17, true, "#8B93A1"); label.VerticalAlignment = VerticalAlignment.Center;
            var content = new Grid(); content.ColumnDefinitions.Add(new() { Width = new(28) }); content.ColumnDefinitions.Add(new());
            var number = Text($"{i + 1:00}", 10, true, "#616B78"); number.VerticalAlignment = VerticalAlignment.Center;
            content.Children.Add(number); Grid.SetColumn(label, 1); content.Children.Add(label);
            var button = new Button { Content = content, Height = 38, Margin = new(0, 0, 0, i + 1 < names.Length ? 9 : 0), Padding = new(14, 0, 0, 0), BorderThickness = new(3, 0, 0, 0), Background = Brushes.Transparent, BorderBrush = Brushes.Transparent };
            button.Template = NavigationTemplate();
            AutomationProperties.SetAutomationId(button, "Nav" + names[i]); AutomationProperties.SetName(button, names[i]);
            button.Click += (_, _) => ShowPage(page);
            button.MouseEnter += (_, _) => { if (currentPage == page) NavigationSelection.Opacity = .85; };
            button.MouseLeave += (_, _) => { if (currentPage == page) NavigationSelection.Opacity = 1; };
            navButtons.Add(button); Navigation.Children.Add(button);
        }
    }
    private static ControlTemplate NavigationTemplate()
    {
        var border = new FrameworkElementFactory(typeof(Border)); border.Name = "Face";
        border.SetBinding(Border.BackgroundProperty, new Binding("Background") { RelativeSource = RelativeSource.TemplatedParent });
        border.SetBinding(Border.BorderBrushProperty, new Binding("BorderBrush") { RelativeSource = RelativeSource.TemplatedParent });
        border.SetBinding(Border.BorderThicknessProperty, new Binding("BorderThickness") { RelativeSource = RelativeSource.TemplatedParent });
        var content = new FrameworkElementFactory(typeof(ContentPresenter)); content.SetValue(FrameworkElement.MarginProperty, new Thickness(11, 0, 0, 0)); content.SetValue(FrameworkElement.VerticalAlignmentProperty, VerticalAlignment.Stretch);
        border.AppendChild(content);
        var template = new ControlTemplate(typeof(Button)) { VisualTree = border };
        var hover = new Trigger { Property = IsMouseOverProperty, Value = true }; hover.Setters.Add(new Setter(Border.OpacityProperty, .85, "Face")); template.Triggers.Add(hover);
        return template;
    }
    private void Adopt(LauncherState incoming)
    {
        bool initial = state.Controls.Count == 0;
        if (initial || !incoming.FoldersOk || (!state.FoldersOk && incoming.FoldersOk))
            foldersExpanded = !incoming.FoldersOk;
        suppress = true;
        foreach (var c in state.Controls) c.PropertyChanged -= SettingChanged;
        state = incoming;
        // Sidebar, window border, page margins, two panels, gap and scrollbar.
        MinWidth = Math.Ceiling(228 + 2 + 72 + 2 * MeasureGameStatus(double.PositiveInfinity).Width + 24 + 10);
        foreach (var c in state.Controls) c.PropertyChanged += SettingChanged;
        pages.Clear(); ShowPage(currentPage, initial && !capture && !perf); suppress = false;
        FooterPlay.IsEnabled = preview || (state.FoldersOk && string.IsNullOrEmpty(state.HardwareProblem));
        FooterPlay.ToolTip = string.IsNullOrEmpty(state.HardwareProblem) ? "Save and start the game." : state.HardwareProblem;
    }
    private async void SettingChanged(object? sender, PropertyChangedEventArgs args)
    {
        if (suppress || sender is not Setting setting || args.PropertyName is not ("Selected" or "Text")) return;
        int changed = ++revision; SetStatus("Unsaved changes");
        state.ChangedId = setting.Id;
        if (args.PropertyName == "Selected")
        {
            string action = setting.Id == state.PresetId ? "preset" : setting.Tag == "@drive_preset" ? "drive-preset" : setting.Tag == "@wheelpreset" ? "wheel-preset" : "validate";
            try { var result = await backend.Call(action, state); if (changed == revision) { Adopt(result); OperationStatus("Unsaved changes"); } }
            catch (Exception ex) { SetStatus(ex.Message, true); }
        }
    }
    private DoubleAnimation Animation(double from, double to, double seconds)
    {
        var a = new DoubleAnimation(from, to, TimeSpan.FromSeconds(seconds)) { EasingFunction = new OriginalMenuEase(), FillBehavior = FillBehavior.HoldEnd };
        Timeline.SetDesiredFrameRate(a, refreshRate); return a;
    }
    private const double PageTransitionSeconds = .280;
    private const double NavigationSelectionSeconds = .220;
    private void MoveNavigationSelection(int index, bool animate)
    {
        NavigationSelection.Opacity = navButtons[index].IsMouseOver ? .85 : 1;
        if (navigationSelection == index) return;
        double target = navButtons[index].TranslatePoint(new Point(), Navigation).Y;
        double from = NavigationSelectionTransform.Y;
        NavigationSelectionTransform.BeginAnimation(TranslateTransform.YProperty, null);
        NavigationSelectionTransform.Y = target;
        NavigationSelection.Visibility = Visibility.Visible;
        if (animate && navigationSelection >= 0)
            NavigationSelectionTransform.BeginAnimation(TranslateTransform.YProperty, PageSlideAnimation(from, target, NavigationSelectionSeconds));
        navigationSelection = index;
    }
    private DoubleAnimation PageSlideAnimation(double from, double to, double seconds = PageTransitionSeconds)
    {
        var animation = new DoubleAnimation(from, to, TimeSpan.FromSeconds(seconds))
        {
            EasingFunction = new CubicEase { EasingMode = EasingMode.EaseInOut },
            FillBehavior = FillBehavior.HoldEnd
        };
        Timeline.SetDesiredFrameRate(animation, refreshRate);
        return animation;
    }
    private void ShowPage(int index, bool animate = true)
    {
        if (state.Controls.Count == 0 || (currentPage == index && PageHolder.Children.Count != 0 && animate)) return;
        currentPage = index;
        for (int i = 0; i < navButtons.Count; ++i)
        {
            var button = navButtons[i]; bool selected = i == index;
            button.Background = Brushes.Transparent;
            button.BorderBrush = Brushes.Transparent;
            var content = (Grid)button.Content;
            ((TextBlock)content.Children[0]).Foreground = Brush(selected ? "#F5F6F8" : "#616B78");
            ((TextBlock)content.Children[1]).Foreground = Brush(selected ? "#F5F6F8" : "#8B93A1");
        }
        MoveNavigationSelection(index, animate);
        FooterPlay.Visibility = index == 0 ? Visibility.Collapsed : Visibility.Visible;
        FooterPlayColumn.Width = new GridLength(index == 0 ? 2 : 184);
        FooterPlay.Content = preview ? "PREVIEW LAUNCH" : "PLAY";
        ResetButton.Visibility = SaveButton.Visibility = index == ModsPage ? Visibility.Collapsed : Visibility.Visible;
        if (!pages.TryGetValue(index, out var page)) { page = BuildPage(index); pages[index] = page; }
        var outgoing = PageHolder.Children.OfType<FrameworkElement>().LastOrDefault();
        PageHolder.Children.Clear();
        if (animate && outgoing != null && outgoing != page)
        {
            outgoing.IsHitTestVisible = false; PageHolder.Children.Add(outgoing);
            outgoing.BeginAnimation(OpacityProperty, Animation(outgoing.Opacity, 0, PageTransitionSeconds));
            var exitTransform = (TranslateTransform)outgoing.RenderTransform;
            exitTransform.BeginAnimation(TranslateTransform.YProperty, PageSlideAnimation(exitTransform.Y, -12));
        }
        page.IsHitTestVisible = true; page.BeginAnimation(OpacityProperty, null); page.Opacity = animate ? 0 : 1;
        var enterTransform = (TranslateTransform)page.RenderTransform;
        enterTransform.BeginAnimation(TranslateTransform.YProperty, null); enterTransform.Y = 0;
        PageHolder.Children.Add(page);
        if (animate)
        {
            var fade = Animation(0, 1, PageTransitionSeconds);
            fade.Completed += (_, _) => { if (PageHolder.Children.Count > 0 && ReferenceEquals(PageHolder.Children[^1], page)) foreach (var old in PageHolder.Children.OfType<FrameworkElement>().Where(x => x != page).ToArray()) PageHolder.Children.Remove(old); };
            page.BeginAnimation(OpacityProperty, fade);
            enterTransform.BeginAnimation(TranslateTransform.YProperty, PageSlideAnimation(32, 0));
        }
        // Reduced page motion: incoming +32 to 0, outgoing 0 to -12,
        // 280 ms cubic ease-in-out slide; opacity keeps the smoothstep curve.
        // Backgrounds only crossfade.
        ChangeBackground(index, animate);
    }
    private void ChangeBackground(int index, bool animate)
    {
        var art = new Grid { ClipToBounds = true };
        var image = new System.Windows.Controls.Image { Source = Image(backgroundPaths[index], true), Stretch = Stretch.UniformToFill };
        RenderOptions.SetBitmapScalingMode(image, BitmapScalingMode.LowQuality); art.Children.Add(image);
        art.Children.Add(new Border { Background = (Brush)FindResource("ArtworkDim") });
        if (!animate || BackgroundHolder.Children.Count == 0) { BackgroundHolder.Children.Clear(); BackgroundHolder.Children.Add(art); return; }
        // Keep the current composited layers beneath the new one, including during rapid navigation.
        BackgroundHolder.Children.Add(art);
        var fade = Animation(0, 1, PageTransitionSeconds);
        fade.Completed += (_, _) =>
        {
            if (BackgroundHolder.Children.Count > 0 && ReferenceEquals(BackgroundHolder.Children[^1], art))
                foreach (var old in BackgroundHolder.Children.OfType<FrameworkElement>().Where(x => x != art).ToArray()) BackgroundHolder.Children.Remove(old);
        };
        art.BeginAnimation(OpacityProperty, fade);
    }
    private static void Place(Canvas canvas, FrameworkElement element, double x, double y, double width, double height)
    {
        element.Width = Math.Max(0, width); element.Height = Math.Max(0, height); Canvas.SetLeft(element, x); Canvas.SetTop(element, y); canvas.Children.Add(element);
    }
    private Border Caption(Canvas canvas, string value, double x, double y, double width, double height, double size = 14, bool heading = false, string color = "#E8EAED", bool wrap = false, bool center = false)
    {
        var text = Text(value, size, heading, color); text.TextWrapping = wrap ? TextWrapping.Wrap : TextWrapping.NoWrap;
        text.VerticalAlignment = VerticalAlignment.Center; text.TextTrimming = TextTrimming.CharacterEllipsis;
        text.TextAlignment = center ? TextAlignment.Center : TextAlignment.Left;
        var box = new Border { Child = text };
        Place(canvas, box, x, y, width, height);
        return box;
    }
    private FrameworkElement BuildPage(int index)
    {
        if (index == ModsPage) return BuildModsPage();
        if (index == 0) return BuildHomePage();
        var canvas = new Canvas { VerticalAlignment = VerticalAlignment.Top, HorizontalAlignment = HorizontalAlignment.Left };
        var scroll = new ScrollViewer { VerticalContentAlignment = VerticalAlignment.Top, HorizontalContentAlignment = HorizontalAlignment.Left, RenderTransform = new TranslateTransform(), Content = canvas, VerticalScrollBarVisibility = ScrollBarVisibility.Auto, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled, PanningMode = PanningMode.VerticalOnly };
        double previousWidth = -1;
        void ArrangePage()
        {
            double width = Math.Max(700, scroll.ViewportWidth > 0 ? scroll.ViewportWidth : scroll.ActualWidth);
            if (Math.Abs(width - previousWidth) < .5) return;
            previousWidth = width; LayoutPage(canvas, index, width);
        }
        scroll.SizeChanged += (_, _) => ArrangePage();
        scroll.ScrollChanged += (_, e) => { if (e.ViewportWidthChange != 0) ArrangePage(); };
        return scroll;
    }
    private FrameworkElement BuildHomePage()
    {
        // Bottom alignment while space is available; the upper section sets a minimum
        // row height and pushes Quick Settings below the viewport in a short window.
        var page = new Grid();
        page.RowDefinitions.Add(new() { MinHeight = 572 });
        page.RowDefinitions.Add(new() { Height = GridLength.Auto });
        var upper = new Canvas { VerticalAlignment = VerticalAlignment.Top, HorizontalAlignment = HorizontalAlignment.Left };
        page.Children.Add(upper);
        var quick = new Canvas();
        var dock = new Border { Child = quick, Margin = new(36, 0, 36, 24), Tag = "QuickSettingsDock" };
        AutomationProperties.SetAutomationId(dock, "QuickSettingsDock");
        Grid.SetRow(dock, 1); page.Children.Add(dock);
        var scroll = new ScrollViewer { Content = page, RenderTransform = new TranslateTransform(), VerticalContentAlignment = VerticalAlignment.Top, HorizontalContentAlignment = HorizontalAlignment.Left, VerticalScrollBarVisibility = ScrollBarVisibility.Auto, HorizontalScrollBarVisibility = ScrollBarVisibility.Disabled };
        double previousWidth = -1;
        void ArrangeHome()
        {
            if (scroll.ActualWidth <= 0 || scroll.ActualHeight <= 0) return;
            double width = Math.Max(700, scroll.ViewportWidth > 0 ? scroll.ViewportWidth : scroll.ActualWidth);
            // Scrollbar appearance must not replace the expander mid-animation.
            if (foldersAnimating && previousWidth > 0) width = previousWidth;
            if (Math.Abs(previousWidth - width) >= .5)
            {
                previousWidth = width; page.Width = width; LayoutPage(upper, 0, width);
                double available = width - 72; quick.Width = available; quick.Children.Clear();
                int columns = available >= 850 ? 4 : 2;
                double qw = Math.Min(MeasureGameStatus(available).Width, (available - (columns - 1) * 16) / columns);
                double start = PanelGroupLeft(width, columns * qw + (columns - 1) * 16) - 36;
                Caption(quick, "QUICK SETTINGS", start, 0, columns * qw + (columns - 1) * 16, 27, 16, true, "#F5F6F8", center: true).Tag = "PageHeading";
                double qy = 36;
                var controls = state.QuickControls.Select(id => state.Controls.Single(c => c.Id == id)).ToArray();
                for (int k = 0; k < controls.Length; k += columns)
                {
                    var line = controls.Skip(k).Take(columns).ToArray();
                    double lineHeight = line.Max(c => RowHeight(c, qw, true));
                    for (int col = 0; col < line.Length; ++col)
                        Place(quick, SettingView(line[col], qw, true), start + col * (qw + 16), qy, qw, lineHeight);
                    qy += lineHeight + 6;
                }
                quick.Height = qy - 6;
            }
            double viewport = scroll.ViewportHeight > 0 ? scroll.ViewportHeight : scroll.ActualHeight;
            page.RowDefinitions[0].MinHeight = upper.Height + 24;
            page.Height = Math.Max(viewport, page.RowDefinitions[0].MinHeight + quick.Height + 24);
        }
        scroll.SizeChanged += (_, _) => ArrangeHome();
        arrangeHome = ArrangeHome;
        upper.SizeChanged += (_, _) => ArrangeHome();
        scroll.ScrollChanged += (_, e) => { if (e.ViewportWidthChange != 0 || e.ViewportHeightChange != 0) ArrangeHome(); };
        return scroll;
    }
    private const double SectionHeaderHeight = 38;
    private sealed record PanelMeasure(string Title, double Width);
    private double PanelGroupLeft(double pageWidth, double groupWidth)
    {
        // Page coordinates begin after the 228-DIP sidebar. Keep the whole
        // window's center as the target, with safe margins in narrow windows.
        double target = Root.ActualWidth / 2 - 228 - groupWidth / 2;
        return Math.Clamp(target, 36, Math.Max(36, pageWidth - 36 - groupWidth));
    }
    private void RulePanel(Canvas canvas, double x, double y, double width, double height, string title)
    {
        Place(canvas, new Border { Background = Brush("#ED2B38"), Tag = new PanelMeasure(title, width) }, x, y, 3, SectionHeaderHeight);
        Caption(canvas, title, x + 20, y, width - 40, SectionHeaderHeight, 15, true, "#D7D7D9");
    }
    private double SliderValueWidth(Setting setting)
    {
        double Measure(int number)
        {
            var text = Text(number + setting.Unit, 14); text.TextWrapping = TextWrapping.NoWrap;
            text.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
            return Math.Ceiling(text.DesiredSize.Width);
        }
        return Math.Max(75, Math.Max(Measure(setting.Minimum), Measure(setting.Maximum)));
    }
    private (double Width, double Height) SettingLabelSize(Setting setting, double width, bool quick)
    {
        double labelWidth = width - (setting.Kind == "Toggle" ? 42 : setting.Kind == "Slider" ? SliderValueWidth(setting) + 12 : 0) - (setting.Details.Length == 0 ? 0 : 26);
        var label = Text(setting.Label, quick ? 14 : 15);
        label.Measure(new Size(Math.Max(1, labelWidth), double.PositiveInfinity));
        return (labelWidth, Math.Max(quick ? 24 : 20, Math.Ceiling(label.DesiredSize.Height)));
    }
    private double RowHeight(Setting setting, double width, bool quick = false)
    {
        if (setting.Kind == "Action") return state.BenchmarkRanDetails.Count > 0 ? 288 : 242;
        double labelHeight = SettingLabelSize(setting, width, quick).Height;
        if (setting.Kind == "Toggle") return Math.Max(52, labelHeight + 12);
        return Math.Max(quick ? 76 : 82, labelHeight + (quick ? 52 : 62));
    }
    private void LayoutPage(Canvas canvas, int index, double availableWidth)
    {
        canvas.Children.Clear(); canvas.Width = availableWidth;
        const double left = 36; double width = availableWidth - 72;
        if (index == 0) { BuildHome(canvas, left, width); return; }
        if (sourcePages[index] < 0) { canvas.Height = 180; return; }
        double y = 145;
        double maxPanelWidth = MeasureGameStatus(width).Width;
        var cards = state.Cards.Where(c => c.Page == sourcePages[index] && (names[index] != "Driving" || c.Subpage == drivingSubpage)).ToArray();
        if (names[index] == "Driving")
        {
            string[] sections = ["Assists and view", "Wheel and force feedback"];
            double sectionWidth = Math.Min(maxPanelWidth, (width - 18) / 2);
            for (int i = 0; i < sections.Length; ++i)
            {
                int section = i;
                var button = new Button { Content = sections[i], Style = (Style)FindResource("FieldButton"), Padding = new(12,0,12,0), Background = drivingSubpage == i ? Brush("#ED2B38") : (Brush)FindResource("OptionInactive"), BorderThickness = new(drivingSubpage == i ? 3 : 0,0,0,0), BorderBrush = Brush("#ED2B38") };
                AutomationProperties.SetAutomationId(button, "DrivingSubpage" + i);
                button.Click += (_, _) => { drivingSubpage = section; pages.Remove(index); ShowPage(index, false); };
                Place(canvas, button, PanelGroupLeft(availableWidth, 2 * sectionWidth + 18) + i * (sectionWidth + 18), y, sectionWidth, 38);
            }
            y += 56;
        }
        double cardWidth = maxPanelWidth;
        int columns = Math.Max(1, Math.Min(cards.Length, (int)Math.Floor((width + 18) / (cardWidth + 18))));
        double groupLeft = PanelGroupLeft(availableWidth, columns * cardWidth + (columns - 1) * 18);
        double groupWidth = columns * cardWidth + (columns - 1) * 18;
        Caption(canvas, names[index].ToUpperInvariant(), groupLeft, 46, groupWidth, 46, 32, true, "#F5F6F8", center: true).Tag = "PageHeading";
        Caption(canvas, hints[index], groupLeft, 99, groupWidth, 24, 15, false, "#8B93A1", center: true).Tag = "PageHeading";
        double[] cy = Enumerable.Repeat(y, columns).ToArray();
        foreach (var card in cards)
        {
            int col = Array.IndexOf(cy, cy.Min()); double x = groupLeft + col * (cardWidth + 18);
            var settings = card.Controls.Select(id => state.Controls.Single(c => c.Id == id)).Where(c => !c.Hidden).ToArray();
            double noteHeight = string.IsNullOrEmpty(card.Note) ? 0 : new FormattedText(card.Note, System.Globalization.CultureInfo.CurrentUICulture, FlowDirection.LeftToRight, new Typeface((FontFamily)Application.Current.FindResource("BodyFont"), FontStyles.Normal, FontWeights.Normal, FontStretches.Normal), 12, Brushes.White, VisualTreeHelper.GetDpi(this).PixelsPerDip) { MaxTextWidth = cardWidth - 44 }.Height + 12;
            double height = SectionHeaderHeight + 12 + 17 + settings.Sum(s => RowHeight(s, cardWidth - 44)) + noteHeight;
            RulePanel(canvas, x, cy[col], cardWidth, height, card.Title);
            double ry = cy[col] + SectionHeaderHeight + 12;
            foreach (var setting in settings) { double rh = RowHeight(setting, cardWidth - 44); Place(canvas, SettingView(setting, cardWidth - 44), x + 22, ry, cardWidth - 44, rh); ry += rh; }
            if (noteHeight > 0) Caption(canvas, card.Note, x + 22, cy[col] + height - noteHeight - 4, cardWidth - 44, noteHeight, 12, false, "#8B93A1", true);
            cy[col] += height + 18;
        }
        canvas.Height = cy.Max() + 10;
    }
    private FrameworkElement SettingView(Setting setting, double width, bool quick = false)
    {
        var row = new Canvas(); double height = RowHeight(setting, width, quick);
        var labelSize = SettingLabelSize(setting, width, quick);
        double fieldTop = labelSize.Height + (quick ? 4 : 12);
        void Label(double top, double boxHeight)
        {
            var label = Text(setting.Label, quick ? 14 : 15, color: "#E8EAED");
            label.TextTrimming = TextTrimming.None;
            AutomationProperties.SetAutomationId(label, "SettingLabel" + setting.Id);
            Place(row, label, 0, top, labelSize.Width, boxHeight);
            if (setting.Details.Length > 0) Place(row, SettingHelp(setting), labelSize.Width + 4, top + Math.Max(0, (boxHeight - 20) / 2), 20, 20);
        }
        FrameworkElement field;
        if (setting.Kind == "Toggle")
        {
            Label((height - 4 - labelSize.Height) / 2, labelSize.Height);
            var check = new CheckBox { ToolTip = setting.Hint, MinHeight = 0, Padding = new(0) };
            check.SetBinding(ToggleButton.IsCheckedProperty, new Binding(nameof(Setting.IsOn)) { Source = setting, Mode = BindingMode.TwoWay }); field = check;
            Place(row, check, width - 31, (height - 4 - 24) / 2, 24, 24);
        }
        else if (setting.Kind == "Action")
        {
            var run = new Button { Content = "RUN BENCHMARK", Style = (Style)FindResource("PlayButton"), FontSize = 16, Padding = new(0) };
            run.Click += async (_, _) => await Launch(true); field = run; Place(row, run, 0, 3, width, 44);
            Caption(row, "LAST RESULT", 0, 65, width, 26, 14, true, "#8B93A1");
            if (!string.IsNullOrEmpty(state.BenchmarkFps))
            {
                Caption(row, state.BenchmarkFps + " FPS", 0, 96, width, 48, 32, true);
                Caption(row, state.BenchmarkLows, 0, 148, width, 26);
                Caption(row, state.BenchmarkAbout, 0, 178, width, 50, 13, false, "#8B93A1", true);
            }
            else Caption(row, "Run an event to measure average FPS and frame-time lows.", 0, 104, width, 70, 16, false, "#8B93A1", true);
            AddBenchmarkDetails(row, width);
        }
        else if (setting.Kind == "Button")
        {
            Label(quick ? 0 : 6, labelSize.Height);
            var button = new Button { Content = setting.Options.FirstOrDefault() ?? setting.Label, Padding = new(12,0,12,0) };
            ConfigureNativeButton(button, setting);
            field = button; Place(row, field, 0, fieldTop, width, 38);
        }
        else if (setting.Kind == "Slider")
        {
            Label(quick ? 0 : 6, labelSize.Height);
            double valueWidth = SliderValueWidth(setting);
            var value = Text(setting.Selected + setting.Unit, 14); value.TextAlignment = TextAlignment.Right; value.TextWrapping = TextWrapping.NoWrap;
            AutomationProperties.SetAutomationId(value, "SettingValue" + setting.Id);
            Place(row, value, width - valueWidth, quick ? 0 : 6, valueWidth, 20);
            var slider = new Slider { Minimum = setting.Minimum, Maximum = setting.Maximum, TickFrequency = 1, IsSnapToTickEnabled = true, Value = setting.Selected, Style = (Style)FindResource("SettingSlider") };
            slider.ValueChanged += (_, _) => { suppress = true; setting.Selected = (int)Math.Round(slider.Value); suppress = false; value.Text = setting.Selected + setting.Unit; ++revision; SetStatus("Unsaved changes"); };
            slider.LostMouseCapture += (_, _) => { if (!slider.IsMouseCaptureWithin && state.Controls.Contains(setting)) SettingChanged(setting, new(nameof(Setting.Selected))); };
            slider.KeyUp += (_, _) => { if (state.Controls.Contains(setting)) SettingChanged(setting, new(nameof(Setting.Selected))); };
            field = slider; Place(row, field, 0, fieldTop, width, 38);
        }
        else
        {
            Label(quick ? 0 : 6, labelSize.Height);
            if (setting.Kind == "Text")
            {
                var box = new TextBox { MaxLength = 20 };
                box.SetBinding(TextBox.TextProperty, new Binding(nameof(Setting.Text)) { Source = setting, Mode = BindingMode.TwoWay, UpdateSourceTrigger = UpdateSourceTrigger.PropertyChanged }); field = box;
            }
            else
            {
                var combo = new ComboBox { ItemsSource = setting.Options };
                combo.SetBinding(Selector.SelectedIndexProperty, new Binding(nameof(Setting.Selected)) { Source = setting, Mode = BindingMode.TwoWay }); field = combo;
            }
            Place(row, field, 0, fieldTop, width, 38);
        }
        if (!quick) Place(row, new Border { Background = Brush("#2E2E31") }, 0, height - 2, width, 1);
        field.IsEnabled = setting.Enabled;
        AutomationProperties.SetName(field, setting.Label); AutomationProperties.SetAutomationId(field, (quick ? "Quick" : "Setting") + setting.Id);
        row.ToolTip = setting.Hint; row.MouseEnter += (_, _) => ShowStatusHint(setting.Hint); row.MouseLeave += (_, _) => RestoreStatus();
        return row;
    }
    private (string[] Labels, string[] Values, double LabelWidth, double ValueOffset, double Width) MeasureGameStatus(double availableWidth)
    {
        string[] labels = ["Game version", "Renderer", "Hardware", "Runtime"];
        string[] values = [state.FoldersOk ? "1.28" : "Select game folders", "Vulkan 1.3  /  native x64", string.IsNullOrEmpty(state.HardwareProblem) ? "No blocking issue" : "Needs attention", state.RuntimeAvailable ? "Available" : "Runtime not bundled"];
        double Measure(string value, double size, bool heading = false)
        {
            var text = Text(value, size, heading); text.TextWrapping = TextWrapping.NoWrap;
            text.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
            return Math.Ceiling(text.DesiredSize.Width);
        }
        double labelWidth = Math.Max(125, labels.Max(value => Measure(value, 14)));
        double valueOffset = 22 + labelWidth + 15;
        double sw = Math.Min(availableWidth, Math.Max(valueOffset + values.Max(value => Measure(value, 14)) + 22,
            Measure(preview ? "PREVIEW LAUNCH" : "PLAY", 23, true) + 44));
        return (labels, values, labelWidth, valueOffset, sw);
    }
    private void BuildHome(Canvas canvas, double left, double width)
    {
        var (labels, values, labelWidth, valueOffset, sw) = MeasureGameStatus(width);
        double foldersWidth = width - sw - 24;
        bool besideStatus = !foldersExpanded || foldersWidth >= sw;
        double actualFoldersWidth = Math.Min(sw, besideStatus ? foldersWidth : width);
        double groupWidth = besideStatus ? sw + 24 + actualFoldersWidth : sw;
        left = PanelGroupLeft(canvas.Width, groupWidth);
        double logoLeft = left + (groupWidth - 500) / 2;
        Place(canvas, new Border { Background = Brush("#454547"), OpacityMask = new ImageBrush(Image("brand/logo-red.png")) { Stretch = Stretch.Fill }, Tag = "PageHeading" }, logoLeft, 62, 500, 73);
        // Match the visible logo edge, excluding its 8-pixel transparent inset.
        double logoInset = 500 * 8d / 880;
        Caption(canvas, "NEVER DRIVE ALONE", logoLeft + logoInset, 151, 500 - logoInset, 29, 22, true, "#F5F6F8").Tag = "PageHeading";
        double statusShift = 48 - SectionHeaderHeight - 12;
        RulePanel(canvas, left, 230, sw, 198 - statusShift, "GAME STATUS");
        for (int k = 0; k < 4; ++k)
        {
            Caption(canvas, labels[k], left + 22, 278 - statusShift + k * 35, labelWidth, 29, 14, false, "#8B93A1");
            var statusValue = Caption(canvas, values[k], left + valueOffset, 278 - statusShift + k * 35, sw - valueOffset - 22, 29, 14, false, k == 0 && state.FoldersOk ? "#5AC878" : "#E8EAED");
            if (k == 2 && !string.IsNullOrEmpty(state.HardwareProblem))
            {
                statusValue.ToolTip = state.HardwareProblem; statusValue.Cursor = Cursors.Hand;
                statusValue.MouseLeftButtonUp += (_, _) => ShowDetails("HARDWARE CHECK", state.HardwareProblem);
                AutomationProperties.SetName(statusValue, state.HardwareProblem);
            }
            if (k == 0 && state.FoldersOk)
            {
                var verified = new DottedCaptionGlyph { Glyph = "Checkmark", Fill = Brush("#5AC878"), ToolTip = "Game files verified" };
                AutomationProperties.SetName(verified, "Game files verified");
                var version = Text(values[0], 14); version.Measure(new Size(double.PositiveInfinity, double.PositiveInfinity));
                Place(canvas, verified, left + valueOffset + Math.Ceiling(version.DesiredSize.Width) + 10, 278 - statusShift + 6.5, 16, 16);
            }
            if (k < 3) Place(canvas, new Border { Background = Brush("#313134") }, left + 22, 310 - statusShift + k * 35, sw - 44, 1);
        }
        var play = new Button { Content = preview ? "PREVIEW LAUNCH" : "PLAY", Style = (Style)FindResource("PlayButton"), FontSize = 23, Padding = new(0), BorderThickness = new(0), Background = Brush("#ED2B38"), IsEnabled = preview || (state.FoldersOk && string.IsNullOrEmpty(state.HardwareProblem)), ToolTip = string.IsNullOrEmpty(state.HardwareProblem) ? "Save and start the game." : state.HardwareProblem };
        play.Click += PlayClick; AutomationProperties.SetAutomationId(play, "HomePlay"); Place(canvas, play, left, 452 - statusShift, sw, 44);
        Caption(canvas, preview ? "Interface preview only. No game will be started." : "Your settings are saved when you start the game.", left, 524 - statusShift, sw, 24, 13, false, "#8B93A1");
        // Paths share the status row on wide windows and flow below it on narrow ones.
        double foldersY = besideStatus ? 230 : 574;
        double foldersHeight = BuildFolders(canvas, besideStatus ? left + sw + 24 : left, foldersY, actualFoldersWidth);
        // Quick Settings stays bottom-docked and is pushed down by this upper section.
        canvas.Height = Math.Max(548, foldersY + foldersHeight);
    }
    private double BuildFolders(Canvas canvas, double left, double y, double width)
    {
        double bodyHeight = 108 + (state.FoldersOk ? 0 : 42);
        bool opening = animateFoldersOpening && foldersExpanded;
        animateFoldersOpening = false;
        var body = new Canvas { Width = width, Height = foldersExpanded && !opening ? bodyHeight : 0, Opacity = foldersExpanded && !opening ? 1 : 0, ClipToBounds = true, Visibility = foldersExpanded ? Visibility.Visible : Visibility.Collapsed };
        AutomationProperties.SetAutomationId(body, "GameFoldersBody");
        body.SizeChanged += (_, _) => canvas.Height = Math.Max(548, y + SectionHeaderHeight + body.ActualHeight);
        Place(canvas, body, left, y + SectionHeaderHeight, width, body.Height);
        var headerContent = new Grid { Width = width - 40, Height = SectionHeaderHeight };
        var title = Text("GAME FILES", 15, true, "#D7D7D9"); title.VerticalAlignment = VerticalAlignment.Center;
        headerContent.Children.Add(title);
        var arrowRotation = new RotateTransform(foldersExpanded ? 180 : 0);
        headerContent.Children.Add(new DottedCaptionGlyph { Glyph = "ChevronDown", Fill = Brush("#B5B5BE"), Width = 16, Height = 16, HorizontalAlignment = HorizontalAlignment.Right, VerticalAlignment = VerticalAlignment.Center, RenderTransformOrigin = new(.5,.5), RenderTransform = arrowRotation });
        var toggle = new Button { Content = headerContent, Padding = new(20,0,20,0), Background = Brushes.Transparent, BorderThickness = new(0), ToolTip = foldersExpanded ? "Hide game folders" : "Show or change game folders" };
        AutomationProperties.SetName(toggle, foldersExpanded ? "Hide game folders" : "Show game folders");
        AutomationProperties.SetAutomationId(toggle, "GameFoldersToggle");
        toggle.Tag = new PanelMeasure("GAME FILES", width);
        int animationRevision = 0;
        void AnimateBody(bool expand)
        {
            int token = ++animationRevision;
            foldersAnimating = true;
            body.Visibility = Visibility.Visible; body.IsHitTestVisible = expand;
            toggle.ToolTip = expand ? "Hide game folders" : "Show or change game folders";
            AutomationProperties.SetName(toggle, expand ? "Hide game folders" : "Show game folders");
            if (expand) body.Height = bodyHeight;
            var fade = new DoubleAnimation(body.Opacity, expand ? 1 : 0, TimeSpan.FromSeconds(.12));
            Timeline.SetDesiredFrameRate(fade, refreshRate);
            fade.Completed += (_, _) =>
            {
                if (token != animationRevision) return;
                foldersAnimating = false;
                if (expand) { arrangeHome?.Invoke(); return; }
                body.Visibility = Visibility.Collapsed;
                pages.Remove(0); ShowPage(0, false);
            };
            body.BeginAnimation(OpacityProperty, fade);
            arrowRotation.Angle = expand ? 180 : 0;
        }
        toggle.Click += (_, _) =>
        {
            if (!foldersExpanded && body.Visibility == Visibility.Collapsed)
            {
                foldersExpanded = true; animateFoldersOpening = true; foldersAnimating = true;
                pages.Remove(0); ShowPage(0, false); return;
            }
            foldersExpanded = !foldersExpanded; AnimateBody(foldersExpanded);
        };
        Place(canvas, toggle, left, y, width, SectionHeaderHeight);
        Place(canvas, new Border { Background = Brush("#ED2B38"), IsHitTestVisible = false }, left, y, 3, SectionHeaderHeight);
        for (int visualRow = 0; visualRow < 2; ++visualRow)
        {
            int i = 1 - visualRow;
            int which = i; double fy = 12 + visualRow * 48, rw = width - 44;
            Caption(body, i == 0 ? "Update 1.28" : "Base game", 22, fy, 96, 38, 14);
            string selectedPath = i == 0 ? state.UpdatePath : state.BasePath;
            var path = new TextBox { Text = selectedPath, ToolTip = selectedPath, IsReadOnly = true, FontSize = 14, BorderThickness = new(0) };
            AutomationProperties.SetName(path, i == 0 ? "Update folder (version 1.28)" : "Base game folder");
            AutomationProperties.SetAutomationId(path, "GameFolder" + i);
            Place(body, path, 126, fy, rw - 154, 38);
            Place(body, new Border { Background = Brush(state.FoldersOk ? "#5AC878" : "#F0AA3C") }, 126, fy, 3, 38);
            var browse = new Button { Content = new DottedCaptionGlyph { Glyph = "Search", Fill = Brushes.White, Width = 20, Height = 20 }, Style = (Style)FindResource("FieldButton"), ToolTip = "Browse", Padding = new(0) };
            AutomationProperties.SetName(browse, i == 0 ? "Browse for update folder" : "Browse for base game folder");
            AutomationProperties.SetAutomationId(browse, "BrowseGameFolder" + i);
            Place(body, browse, 22 + rw - 38, fy, 38, 38);
            browse.Click += async (_, _) =>
            {
                var dialog = new OpenFolderDialog { Title = which == 0 ? "Choose the DRIVECLUB v1.28 update" : "Choose the DRIVECLUB base game" };
                if (dialog.ShowDialog(this) != true) return;
                if (which == 0) state.UpdatePath = dialog.FolderName; else state.BasePath = dialog.FolderName;
                try { Adopt(await backend.Call("validate", state)); SetStatus(state.FoldersOk ? "Game folders found. Unsaved changes." : state.FolderProblem, !state.FoldersOk); } catch (Exception ex) { SetStatus(ex.Message, true); }
            };
        }
        if (!state.FoldersOk) Caption(body, state.FolderProblem, 22, 108, width - 44, 42, 11, false, "#F0AA3C", true);
        if (opening) body.Loaded += (_, _) => AnimateBody(true);
        return SectionHeaderHeight + (foldersExpanded && !opening ? bodyHeight : 0);
    }
    private async void SaveClick(object? sender, RoutedEventArgs e)
    {
        if (busy) return;
        try { SaveButton.IsEnabled = false; Adopt(await backend.Call("apply", state)); OperationStatus("Saved."); }
        catch (Exception ex) { SetStatus(ex.Message, true); }
        finally { SaveButton.IsEnabled = true; }
    }
    private async void ResetClick(object? sender, RoutedEventArgs e)
    {
        if (busy || currentPage == ModsPage) return;
        ++revision;
        state.ResetPage = sourcePages[currentPage];
        state.ResetSubpage = names[currentPage] == "Driving" ? drivingSubpage : -1;
        string pageName = names[currentPage] == "Driving" ? drivingSubpage == 0 ? "Assists and view" : "Wheel and force feedback" : names[currentPage];
        try { ResetButton.IsEnabled = false; Adopt(await backend.Call("reset-page", state)); SetStatus($"{pageName} defaults restored. Unsaved changes."); }
        catch (Exception ex) { SetStatus(ex.Message, true); }
        finally { ResetButton.IsEnabled = true; }
    }
    private async void PlayClick(object? sender, RoutedEventArgs e) => await Launch(false);
    private void MinimizeClick(object sender, RoutedEventArgs e) => SystemCommands.MinimizeWindow(this);
    private void MaximizeClick(object sender, RoutedEventArgs e) { if (WindowState == WindowState.Maximized) SystemCommands.RestoreWindow(this); else SystemCommands.MaximizeWindow(this); }
    private void CloseClick(object sender, RoutedEventArgs e) => Close();
    private void OpenLaunchSplash()
    {
        busyOverlayParent = (Panel)BusyOverlay.Parent;
        busyOverlayParent.Children.Remove(BusyOverlay);
        double width = Math.Min(960, ActualWidth), height = Math.Min(600, ActualHeight);
        mayCloseSplash = false;
        launchSplash = new Window
        {
            Title = "DRIVECLUB - Loading", Icon = Icon, Width = width, Height = height,
            Left = Left + (ActualWidth - width) / 2, Top = Top + (ActualHeight - height) / 2,
            WindowStartupLocation = WindowStartupLocation.Manual,
            WindowStyle = WindowStyle.None, ResizeMode = ResizeMode.NoResize,
            Background = Brush("#101014"), Topmost = true, ShowInTaskbar = true,
            FontFamily = FontFamily, UseLayoutRounding = true, SnapsToDevicePixels = true,
            Content = BusyOverlay
        };
        switchToGame = new Button
        {
            Content = "Switch to game", Style = (Style)FindResource("FieldButton"),
            HorizontalAlignment = HorizontalAlignment.Right, VerticalAlignment = VerticalAlignment.Top,
            Margin = new Thickness(24), Padding = new Thickness(16, 0, 16, 0),
            Height = 36, FontSize = 12, Visibility = Visibility.Collapsed
        };
        switchToGame.Click += (_, _) => launchSession?.SwitchToGame();
        BusyOverlay.Children.Add(switchToGame);
        launchSplash.Closing += (_, e) =>
        {
            if (mayCloseSplash) return;
            e.Cancel = true;
            if (launchedGameId > 0) launchSession?.SwitchToGame();
        };
        Hide();
        launchSplash.Show();
        launchSplash.UpdateLayout();
    }
    private void CloseLaunchSplash()
    {
        if (launchSplash == null) return;
        mayCloseSplash = true;
        launchSplash.Content = null;
        launchSplash.Close();
        launchSplash = null;
        if (switchToGame != null) BusyOverlay.Children.Remove(switchToGame);
        switchToGame = null;
        busyOverlayParent?.Children.Add(BusyOverlay);
        busyOverlayParent = null;
    }
    private async Task FadeLaunchSplash()
    {
        if (launchSplash == null) return;
        var finished = new TaskCompletionSource<bool>(TaskCreationOptions.RunContinuationsAsynchronously);
        var fade = new DoubleAnimation(1, 0, TimeSpan.FromMilliseconds(160));
        fade.Completed += (_, _) => finished.TrySetResult(true);
        launchSplash.BeginAnimation(OpacityProperty, fade);
        await finished.Task;
    }
    [DllImport("user32.dll")] private static extern bool SetForegroundWindow(IntPtr hwnd);
    [DllImport("user32.dll")] private static extern bool AllowSetForegroundWindow(uint processId);
    private static void FocusLaunchedGame(int processId)
    {
        try
        {
            using var game = Process.GetProcessById(processId);
            game.Refresh();
            if (!game.HasExited && game.MainWindowHandle != IntPtr.Zero)
            {
                AllowSetForegroundWindow((uint)processId);
                SetForegroundWindow(game.MainWindowHandle);
            }
        }
        catch (ArgumentException) { } catch (InvalidOperationException) { }
    }
    private void StopLoadingAnimation()
    {
        spinnerTimer?.Stop();
        ((RotateTransform)Spinner.RenderTransform).BeginAnimation(RotateTransform.AngleProperty, null);
    }
    private async Task Launch(bool benchmark)
    {
        if (busy) return;
        busy = true; CloseButton.IsEnabled = SaveButton.IsEnabled = ResetButton.IsEnabled = false;
        launchedGameId = 0;
        BusyOverlay.Visibility = Visibility.Visible;
        Progress.IsIndeterminate = false; Progress.Value = 0; PercentText.Text = "0%";
        ProgressText.Text = preview ? "Interface preview. No game is being prepared or launched." : "Preparing your game files...";
        Task<LauncherState>? operation = null;
        try
        {
            OpenLaunchSplash();
            StartLoadingAnimation();
            if (preview)
            {
                for (int i = 0; i <= 100; i += 5) { await Task.Delay(160); Progress.Value = i; PercentText.Text = i + "%"; }
                CloseLaunchSplash(); Show(); Activate(); SetStatus("Preview finished. No game was started.");
            }
            else
            {
                launchSession = new LaunchSession();
                var started = new TaskCompletionSource<int>(TaskCreationOptions.RunContinuationsAsynchronously);
                var gameStarted = new Progress<int>(pid =>
                {
                    launchedGameId = pid;
                    Progress.IsIndeterminate = true; PercentText.Text = "";
                    ProgressText.Text = state.StartupSignalSupported ? "Loading DRIVECLUB..." : "DRIVECLUB is starting. Switch to its window when ready.";
                    if (switchToGame != null) switchToGame.Visibility = Visibility.Visible;
                    started.TrySetResult(pid);
                });
                var progress = new Progress<(int Percent, string Text)>(p =>
                {
                    if (launchedGameId == 0) { Progress.Value = p.Percent; PercentText.Text = p.Percent + "%"; ProgressText.Text = p.Text; }
                });
                operation = backend.Call(benchmark ? "benchmark" : "run", state, progress, launchSession.EventName, gameStarted);
                if (benchmark)
                {
                    // The benchmark backend stays alive until the run finishes. Read its child PID
                    // from progress, so waiting for that backend cannot keep the splash over the race.
                    if (await Task.WhenAny(operation, started.Task) == started.Task)
                    {
                        using var stopWaiting = new CancellationTokenSource();
                        var handoff = launchSession.WaitForFrameAsync(await started.Task, TimeSpan.FromMinutes(3), stopWaiting.Token);
                        if (await Task.WhenAny(operation, handoff) == handoff)
                        {
                            await handoff;
                            await FadeLaunchSplash(); CloseLaunchSplash(); StopLoadingAnimation();
                            FocusLaunchedGame(launchedGameId);
                        }
                        else
                        {
                            stopWaiting.Cancel();
                            try { await handoff; } catch (OperationCanceledException) { } catch (IOException) { }
                        }
                    }
                    Adopt(await operation);
                    CloseLaunchSplash(); Show(); Activate(); SetStatus("Benchmark complete.");
                }
                else
                {
                    Adopt(await operation);
                    launchedGameId = state.GameProcessId;
                    if (launchedGameId <= 0) throw new IOException("The launcher backend did not return the game process. Update the complete launcher package.");
                    Progress.IsIndeterminate = true; PercentText.Text = "";
                    ProgressText.Text = state.StartupSignalSupported ? "Loading DRIVECLUB..." : "DRIVECLUB is starting. Switch to its window when ready.";
                    if (switchToGame != null) switchToGame.Visibility = Visibility.Visible;
                    await launchSession.WaitForFrameAsync(launchedGameId, TimeSpan.FromMinutes(3));
                    await FadeLaunchSplash(); CloseLaunchSplash();
                    FocusLaunchedGame(launchedGameId);
                    allowClose = true; Close(); return;
                }
            }
        }
        catch (Exception ex)
        {
            CloseLaunchSplash(); StopLoadingAnimation(); BusyOverlay.Visibility = Visibility.Collapsed;
            Show(); Activate(); SetStatus(ex.Message, true);
            // Keep the IPC operation owned/observed; a timeout never kills an active benchmark.
            if (operation != null && !operation.IsCompleted) try { await operation; } catch (Exception) { }
        }
        finally
        {
            launchSession?.Dispose(); launchSession = null;
            CloseLaunchSplash(); StopLoadingAnimation();
            BusyOverlay.Visibility = Visibility.Collapsed; Progress.IsIndeterminate = false;
            busy = false; CloseButton.IsEnabled = SaveButton.IsEnabled = ResetButton.IsEnabled = true;
        }
    }
    private Storyboard StartLoadingAnimation()
    {
        double width = Math.Max(960, BusyOverlay.ActualWidth), height = Math.Max(600, BusyOverlay.ActualHeight);
        SwipeLayer.Visibility = Visibility.Visible;
        var first = (TranslateTransform)SwipeOne.RenderTransform;
        var second = (TranslateTransform)((TransformGroup)SwipeTwo.RenderTransform).Children[1];
        first.X = -width; first.Y = height;
        second.X = width; second.Y = -height;
        const double movementStart = .3, movementSeconds = .3, landingSeconds = movementStart + movementSeconds;
        var intro = new Storyboard { Duration = TimeSpan.FromSeconds(1.6) };
        Timeline.SetDesiredFrameRate(intro, refreshRate);
        void Add(DependencyObject target, PropertyPath property, AnimationTimeline animation)
        {
            Storyboard.SetTarget(animation, target);
            Storyboard.SetTargetProperty(animation, property);
            intro.Children.Add(animation);
        }
        foreach (var tuple in new[] { (SwipeOne, -width, height, "RenderTransform.X", "RenderTransform.Y"), (SwipeTwo, width, -height, "RenderTransform.Children[1].X", "RenderTransform.Children[1].Y") })
        {
            var x = Animation(tuple.Item2, 0, movementSeconds); x.BeginTime = TimeSpan.FromSeconds(movementStart);
            var y = Animation(tuple.Item3, 0, movementSeconds); y.BeginTime = TimeSpan.FromSeconds(movementStart);
            Add(tuple.Item1, new PropertyPath(tuple.Item4), x);
            Add(tuple.Item1, new PropertyPath(tuple.Item5), y);
        }
        foreach (var swipe in new[] { SwipeOne, SwipeTwo })
        {
            var alpha = new DoubleAnimationUsingKeyFrames { Duration = TimeSpan.FromSeconds(1.6) };
            alpha.KeyFrames.Add(new LinearDoubleKeyFrame(0, KeyTime.FromTimeSpan(TimeSpan.Zero)));
            alpha.KeyFrames.Add(new LinearDoubleKeyFrame(1, KeyTime.FromTimeSpan(TimeSpan.FromSeconds(landingSeconds))));
            alpha.KeyFrames.Add(new LinearDoubleKeyFrame(0, KeyTime.FromTimeSpan(TimeSpan.FromSeconds(1.1))));
            Add(swipe, new PropertyPath(OpacityProperty), alpha);
        }
        var scale = (ScaleTransform)Stamp.RenderTransform;
        var scaleX = Animation(4, 1, movementSeconds); scaleX.BeginTime = TimeSpan.FromSeconds(movementStart);
        var scaleY = Animation(4, 1, movementSeconds); scaleY.BeginTime = TimeSpan.FromSeconds(movementStart);
        Add(Stamp, new PropertyPath("RenderTransform.ScaleX"), scaleX);
        Add(Stamp, new PropertyPath("RenderTransform.ScaleY"), scaleY);
        var stampAlpha = new DoubleAnimationUsingKeyFrames { Duration = TimeSpan.FromSeconds(1.6) };
        stampAlpha.KeyFrames.Add(new LinearDoubleKeyFrame(0, KeyTime.FromTimeSpan(TimeSpan.Zero)));
        stampAlpha.KeyFrames.Add(new LinearDoubleKeyFrame(0, KeyTime.FromTimeSpan(TimeSpan.FromSeconds(movementStart))));
        stampAlpha.KeyFrames.Add(new LinearDoubleKeyFrame(1, KeyTime.FromTimeSpan(TimeSpan.FromSeconds(landingSeconds))));
        stampAlpha.KeyFrames.Add(new LinearDoubleKeyFrame(1, KeyTime.FromTimeSpan(TimeSpan.FromSeconds(1.1))));
        stampAlpha.KeyFrames.Add(new LinearDoubleKeyFrame(0, KeyTime.FromTimeSpan(TimeSpan.FromSeconds(1.2))));
        Add(Stamp, new PropertyPath(OpacityProperty), stampAlpha);
        // Preserve the original plate travel (0.3 to 0.6 s); move the stamp landing to match it.
        intro.Begin(this, HandoffBehavior.SnapshotAndReplace, Environment.GetCommandLineArgs().Contains("--capture-loading-frames"));
        if (spinnerFrames.Count == 0) return intro;
        spinnerIndex = 0; Spinner.Source = spinnerFrames[0];
        spinnerTimer?.Stop(); spinnerTimer = new DispatcherTimer(DispatcherPriority.Render) { Interval = TimeSpan.FromMilliseconds(33) };
        spinnerTimer.Tick += (_, _) => { Spinner.Source = spinnerFrames[++spinnerIndex % spinnerFrames.Count]; }; spinnerTimer.Start();
        return intro;
    }
    private async Task CapturePages()
    {
        string[] args = Environment.GetCommandLineArgs(); int at = Array.IndexOf(args, "--capture");
        string directory = args[at + 1]; Directory.CreateDirectory(directory);
        var details = new Canvas { Width = 430, Height = 130, Background = Brush("#202024"), HorizontalAlignment = HorizontalAlignment.Left, VerticalAlignment = VerticalAlignment.Top };
        Place(details, new DottedCaptionGlyph { Glyph = "Search", Fill = Brushes.White }, 16, 15, 20, 20);
        Place(details, new DottedCaptionGlyph { Glyph = "ChevronDown", Fill = Brushes.White }, 64, 17, 16, 16);
        Place(details, new DottedCaptionGlyph { Glyph = "Minimize", Fill = Brushes.White }, 112, 17, 16, 16);
        Place(details, new DottedCaptionGlyph { Glyph = "Close", Fill = Brushes.White }, 160, 17, 16, 16);
        var sampleField = new ComboBox { Width = 303, Height = 38 };
        AutomationProperties.SetAutomationId(sampleField, "GrainSample");
        Place(details, sampleField, 16, 62, 303, 38);
        Grid.SetRowSpan(details, 3); Panel.SetZIndex(details, 30); Root.Children.Add(details); Root.UpdateLayout();
        var sampleToggle = (ToggleButton)sampleField.Template.FindName("DropToggle", sampleField);
        var sampleGrain = (FilmGrainOverlay)sampleToggle.Template.FindName("DropGrain", sampleToggle);
        // RenderTargetBitmap includes a Canvas child's layout offset.
        Canvas.SetLeft(sampleField, 0); Canvas.SetTop(sampleField, 0); Root.UpdateLayout();
        double sampleGrainOpacity = sampleGrain.Opacity;
        sampleGrain.Opacity = 0; SaveVisual(Path.Combine(directory, "field-base.png"), sampleField);
        sampleGrain.Opacity = sampleGrainOpacity; SaveVisual(Path.Combine(directory, "field-grain.png"), sampleField);
        sampleField.IsDropDownOpen = true; Root.UpdateLayout();
        double grainWhileOpen = sampleGrain.Opacity;
        sampleField.IsDropDownOpen = false; Root.UpdateLayout();
        SaveVisual(Path.Combine(directory, "field-after-interaction.png"), sampleField);
        details.Children.Remove(sampleField);
        sampleField = new ComboBox { Width = 303, Height = 38 };
        AutomationProperties.SetAutomationId(sampleField, "GrainSample");
        Place(details, sampleField, 0, 0, 303, 38); Root.UpdateLayout();
        SaveVisual(Path.Combine(directory, "field-recreated.png"), sampleField);
        sampleToggle = (ToggleButton)sampleField.Template.FindName("DropToggle", sampleField);
        sampleGrain = (FilmGrainOverlay)sampleToggle.Template.FindName("DropGrain", sampleToggle);
        Canvas.SetLeft(sampleField, 16); Canvas.SetTop(sampleField, 62); Root.UpdateLayout();
        SaveVisual(Path.Combine(directory, "control-details.png"), details);
        var optionBrush = (ImageBrush)FindResource("OptionInactive");
        await File.WriteAllTextAsync(Path.Combine(directory, "field-style.json"), JsonSerializer.Serialize(new
        {
            SpriteViewbox = new { optionBrush.Viewbox.X, optionBrush.Viewbox.Y, optionBrush.Viewbox.Width, optionBrush.Viewbox.Height },
            SampleFocusVisualIsNull = sampleField.FocusVisualStyle == null,
            GrainDevicePixels = true, GrainOpacity = sampleGrainOpacity, GrainWhileOpen = grainWhileOpen,
            StablePattern = sampleGrain.StablePattern, DpiScale = VisualTreeHelper.GetDpi(sampleField).DpiScaleX,
            SearchSize = 20, DotSpacing = 16d / 5, DotRadius = 16d / 5 * .29, HandleDots = 2
        }, Backend.JsonOptions));
        Root.Children.Remove(details);
        // Exercise the actual slider template at both endpoints without changing
        // any bound game settings or invoking the backend.
        var sliderBounds = new List<object>();
        foreach (double number in new[] { 0d, 50d, 100d })
        {
            var probe = new Slider { Minimum = 0, Maximum = 100, Value = number, Width = 303, Height = 38, Style = (Style)FindResource("SettingSlider") };
            probe.Measure(new Size(303, 38)); probe.Arrange(new Rect(0, 0, 303, 38)); probe.UpdateLayout();
            var rail = (Border)probe.Template.FindName("TrackRail", probe);
            var track = (Track)probe.Template.FindName("PART_Track", probe);
            var railBounds = rail.TransformToAncestor(probe).TransformBounds(new Rect(rail.RenderSize));
            double thumbCenter = track.Thumb.TranslatePoint(new Point(track.Thumb.ActualWidth / 2, track.Thumb.ActualHeight / 2), probe).X;
            sliderBounds.Add(new { Value = number, Width = probe.ActualWidth, RailLeft = railBounds.Left, RailRight = railBounds.Right, ThumbCenter = thumbCenter, Clipped = probe.ClipToBounds });
            SaveVisual(Path.Combine(directory, $"slider-{number:0}.png"), probe);
        }
        await File.WriteAllTextAsync(Path.Combine(directory, "slider-bounds.json"), JsonSerializer.Serialize(sliderBounds, Backend.JsonOptions));
        var panelWidths = new List<object>();
        object LayoutReport(string pageName, Canvas canvas, Canvas? quick = null)
        {
            var panels = canvas.Children.OfType<FrameworkElement>().Where(c => c.Tag is PanelMeasure).ToArray();
            double leftEdge = panels.Min(Canvas.GetLeft), rightEdge = panels.Max(c => Canvas.GetLeft(c) + ((PanelMeasure)c.Tag).Width);
            object[] Rows(Canvas parent) => parent.Children.OfType<Canvas>().SelectMany(row => row.Children.OfType<TextBlock>()
                .Where(t => AutomationProperties.GetAutomationId(t).StartsWith("SettingLabel"))
                .Select(label =>
                {
                    var measured = Text(label.Text, label.FontSize); measured.Measure(new Size(label.ActualWidth, double.PositiveInfinity));
                    var field = row.Children.OfType<Control>().Single(c => !AutomationProperties.GetAutomationId(c).StartsWith("SettingHelp"));
                    var value = row.Children.OfType<TextBlock>().FirstOrDefault(t => AutomationProperties.GetAutomationId(t).StartsWith("SettingValue"));
                    return (object)new { Label = label.Text, Width = row.ActualWidth, Height = row.ActualHeight,
                        LabelHeight = label.ActualHeight, RequiredLabelHeight = measured.DesiredSize.Height,
                        FieldTop = Canvas.GetTop(field), LabelBottom = Canvas.GetTop(label) + label.ActualHeight,
                        SideBySide = field is CheckBox,
                        ValueGap = value == null ? (double?)null : Canvas.GetLeft(value) - (Canvas.GetLeft(label) + label.ActualWidth),
                        FieldBottom = Canvas.GetTop(field) + field.ActualHeight,
                        TextWrapping = label.TextWrapping.ToString(), Trimming = label.TextTrimming.ToString() };
                })).ToArray();
            var quickRows = quick?.Children.OfType<Canvas>().ToArray() ?? [];
            object[] Headings(Canvas parent, double targetCenter) => parent.Children.OfType<FrameworkElement>()
                .Where(c => Equals(c.Tag, "PageHeading"))
                .Select(c => (object)new { Text = c is Border { Child: TextBlock t } ? t.Text : "Home logo",
                    X = Canvas.GetLeft(c), Y = Canvas.GetTop(c), Width = c.ActualWidth, Height = c.ActualHeight,
                    CenterDelta = Canvas.GetLeft(c) + c.ActualWidth / 2 - targetCenter,
                    Alignment = c is Border { Child: TextBlock label } ? label.TextAlignment.ToString() : "Centered image" }).ToArray();
            return new { Page = pageName, Maximum = MeasureGameStatus(double.PositiveInfinity).Width,
                Panels = panels.Select(c => new { ((PanelMeasure)c.Tag).Title, Width = ((PanelMeasure)c.Tag).Width, X = Canvas.GetLeft(c) }).ToArray(),
                CenterDelta = (leftEdge + rightEdge - canvas.Width) / 2,
                WindowCenterDelta = canvas.TranslatePoint(new Point((leftEdge + rightEdge) / 2, 0), Root).X - Root.ActualWidth / 2,
                WindowCenteredLeft = Root.ActualWidth / 2 - 228 - (rightEdge - leftEdge) / 2,
                ActualGroupLeft = leftEdge,
                ExpectedGroupLeft = PanelGroupLeft(canvas.Width, rightEdge - leftEdge),
                Headings = Headings(canvas, (leftEdge + rightEdge) / 2),
                QuickHeadings = quickRows.Length == 0 ? [] : Headings(quick!, (quickRows.Min(Canvas.GetLeft) + quickRows.Max(c => Canvas.GetLeft(c) + c.ActualWidth)) / 2),
                Rows = Rows(canvas), QuickRows = quick == null ? [] : Rows(quick),
                QuickCenterDelta = quickRows.Length == 0 ? 0 : (quickRows.Min(Canvas.GetLeft) + quickRows.Max(c => Canvas.GetLeft(c) + c.ActualWidth) - quick!.Width) / 2,
                QuickWindowCenterDelta = quickRows.Length == 0 ? 0 : quick!.TranslatePoint(new Point((quickRows.Min(Canvas.GetLeft) + quickRows.Max(c => Canvas.GetLeft(c) + c.ActualWidth)) / 2, 0), Root).X - Root.ActualWidth / 2,
                MinimumWindowWidth = MinWidth, ViewportWidth = canvas.Width };
        }
        for (int i = 0; i < names.Length; ++i)
        {
            ShowPage(i, false); await Dispatcher.InvokeAsync(() => { }, DispatcherPriority.ApplicationIdle); await Task.Delay(100);
            SaveVisual(Path.Combine(directory, names[i].ToLowerInvariant() + ".png"));
            if (i == ModsPage) continue;
            var canvas = i == 0 ? ((Grid)((ScrollViewer)pages[i]).Content).Children.OfType<Canvas>().Single() : (Canvas)((ScrollViewer)pages[i]).Content;
            var quick = i == 0 ? (Canvas)((Grid)((ScrollViewer)pages[i]).Content).Children.OfType<Border>().Single().Child : null;
            panelWidths.Add(LayoutReport(names[i], canvas, quick));
            if (i != 0) SaveVisual(Path.Combine(directory, names[i].ToLowerInvariant() + "-settings.png"), canvas);
        }
        await File.WriteAllTextAsync(Path.Combine(directory, "panel-widths.json"), JsonSerializer.Serialize(panelWidths, Backend.JsonOptions));
        ShowPage(Array.IndexOf(names, "Driving"), false); Root.UpdateLayout();
        var drivingCanvas = (Canvas)((ScrollViewer)pages[Array.IndexOf(names, "Driving")]).Content;
        var presetCombo = drivingCanvas.Children.OfType<Canvas>().SelectMany(row => row.Children.OfType<ComboBox>()).Single(c => AutomationProperties.GetName(c) == "Driving preset");
        presetCombo.Focus(); Root.UpdateLayout(); SaveVisual(Path.Combine(directory, "driving-focused.png"));
        var focusControls = drivingCanvas.Children.OfType<Canvas>().SelectMany(row => row.Children.OfType<Control>()).Concat([SaveButton, ResetButton, MinimizeButton, MaximizeButton, CloseButton]);
        await File.WriteAllTextAsync(Path.Combine(directory, "focus-styles.json"), JsonSerializer.Serialize(focusControls.Select(c => new { Type = c.GetType().Name, Name = AutomationProperties.GetName(c), FocusVisualIsNull = c.FocusVisualStyle == null }).ToArray(), Backend.JsonOptions));
        presetCombo.IsDropDownOpen = true; await Task.Delay(100);
        SaveVisual(Path.Combine(directory, "driving-dropdown-open.png"));
        SaveVisual(Path.Combine(directory, "driving-dropdown.png"), (FrameworkElement)((Popup)presetCombo.Template.FindName("PART_Popup", presetCombo)).Child);
        presetCombo.IsDropDownOpen = false;
        if (state.FoldersOk)
        {
            ShowPage(0, false); Root.UpdateLayout();
            Canvas HomeCanvas() => ((Grid)((ScrollViewer)pages[0]).Content).Children.OfType<Canvas>().Single();
            Canvas FolderBody() => HomeCanvas().Children.OfType<Canvas>().Single(c => AutomationProperties.GetAutomationId(c) == "GameFoldersBody");
            int FolderRows() => FolderBody().Visibility == Visibility.Visible && FolderBody().Height > 0 ? FolderBody().Children.OfType<TextBox>().Count() : 0;
            void ClickFolders() => HomeCanvas().Children.OfType<Button>().Single(c => AutomationProperties.GetAutomationId(c) == "GameFoldersToggle").RaiseEvent(new RoutedEventArgs(Button.ClickEvent));
            int initialRows = FolderRows(); ClickFolders();
            await Dispatcher.InvokeAsync(() => { }, DispatcherPriority.ApplicationIdle); await Task.Delay(45);
            var openingSample = new { Height = FolderBody().Height, Opacity = FolderBody().Opacity };
            SaveVisual(Path.Combine(directory, "play-folders-opening.png")); await Task.Delay(350);
            int expandedRows = FolderRows(); SaveVisual(Path.Combine(directory, "play-folders-open.png"));
            var browseButtons = FolderBody().Children.OfType<Button>().Select(button => new
            {
                Id = AutomationProperties.GetAutomationId(button), Width = button.ActualWidth, Height = button.ActualHeight,
                ToolTip = button.ToolTip?.ToString(), Background = button.Background.ToString(),
                Glyph = ((DottedCaptionGlyph)button.Content).Glyph, Fill = ((DottedCaptionGlyph)button.Content).Fill.ToString()
            }).ToArray();
            await File.WriteAllTextAsync(Path.Combine(directory, "browse-buttons.json"), JsonSerializer.Serialize(new { SaveBackground = SaveButton.Background.ToString(), Buttons = browseButtons }, Backend.JsonOptions));
            ClickFolders(); await Task.Delay(45);
            var closingSample = new { Height = FolderBody().Height, Opacity = FolderBody().Opacity };
            SaveVisual(Path.Combine(directory, "play-folders-closing.png")); await Task.Delay(350);
            int collapsedRows = FolderRows();
            ClickFolders(); await Task.Delay(80); ClickFolders(); await Task.Delay(60); ClickFolders(); await Task.Delay(400);
            int reversedRows = FolderRows(); ClickFolders(); await Task.Delay(400);
            var original = state;
            var invalid = JsonSerializer.Deserialize<LauncherState>(JsonSerializer.Serialize(state, Backend.JsonOptions), Backend.JsonOptions)!;
            invalid.FoldersOk = false; invalid.FolderProblem = "Choose valid game folders.";
            Adopt(invalid); Root.UpdateLayout(); int invalidRows = FolderRows();
            Adopt(original); Root.UpdateLayout(); int validAgainRows = FolderRows();
            await File.WriteAllTextAsync(Path.Combine(directory, "game-folders-collapse.json"), JsonSerializer.Serialize(new { InitialRows = initialRows, ExpandedRows = expandedRows, CollapsedRows = collapsedRows, InvalidPathsRows = invalidRows, ValidAgainRows = validAgainRows, ReversedRows = reversedRows, OpeningSample = openingSample, ClosingSample = closingSample, DurationMs = 120, HeightAnimation = false, InlineLayout = true }, Backend.JsonOptions));
        }
        int driving = Array.IndexOf(names, "Driving");
        drivingSubpage = 1; pages.Remove(driving); ShowPage(driving, false);
        await Dispatcher.InvokeAsync(() => { }, DispatcherPriority.ApplicationIdle); await Task.Delay(100);
        SaveVisual(Path.Combine(directory, "driving-wheel.png"));
        panelWidths.Add(LayoutReport("Driving wheel", (Canvas)((ScrollViewer)pages[driving]).Content));
        await File.WriteAllTextAsync(Path.Combine(directory, "panel-widths.json"), JsonSerializer.Serialize(panelWidths, Backend.JsonOptions));
        drivingSubpage = 0; pages.Remove(driving);
        ShowPage(0, false); Root.UpdateLayout();
        double menuCenter = Navigation.PointToScreen(new Point(0, Navigation.ActualHeight / 2)).Y;
        double sidebarCenter = SidebarMenuHost.PointToScreen(new Point(0, SidebarMenuHost.ActualHeight / 2)).Y;
        await File.WriteAllTextAsync(Path.Combine(directory, "menu-style.json"), JsonSerializer.Serialize(new
        {
            HeadingFont = ((FontFamily)FindResource("HeadingFont")).Source,
            LeftInset = Navigation.TranslatePoint(new Point(), SidebarMenuHost).X,
            Items = navButtons.Select(button => new
            {
                Text = ((TextBlock)((Grid)button.Content).Children[1]).Text,
                Font = ((TextBlock)((Grid)button.Content).Children[1]).FontFamily.Source,
                Size = ((TextBlock)((Grid)button.Content).Children[1]).FontSize,
                Height = button.Height, BottomGap = button.Margin.Bottom, PaddingLeft = button.Padding.Left
            }).ToArray(),
            MainDim = ((Border)((Grid)BackgroundHolder.Children[^1]).Children[1]).Background.ToString(),
            LoadingDim = BusyDim.Fill.ToString()
        }, Backend.JsonOptions));
        await File.WriteAllTextAsync(Path.Combine(directory, "sidebar-menu-layout.json"), JsonSerializer.Serialize(new { MenuCenterPixels = menuCenter, SidebarCenterPixels = sidebarCenter, CenterDifferencePixels = menuCenter - sidebarCenter }, Backend.JsonOptions));
        var dock = ((Grid)((ScrollViewer)pages[0]).Content).Children.OfType<Border>().Single(x => Equals(x.Tag, "QuickSettingsDock"));
        var dockBottom = dock.PointToScreen(new Point(0, dock.ActualHeight)).Y;
        var footerTop = FooterFrame.PointToScreen(new Point(0, 0)).Y;
        var homeUpper = ((Grid)((ScrollViewer)pages[0]).Content).Children.OfType<Canvas>().Single();
        await File.WriteAllTextAsync(Path.Combine(directory, "quick-settings-layout.json"), JsonSerializer.Serialize(new { DockedToBottomWhenRoomAvailable = true, GapAboveFooterPixels = footerTop - dockBottom, ExpectedGapPixels = 24 * VisualTreeHelper.GetDpi(this).DpiScaleY, ScrollsWithUpperPage = true, UpperContentPushesQuickSettings = true, QuickTopDip = ((Grid)((ScrollViewer)pages[0]).Content).RowDefinitions[0].ActualHeight, UpperContentHeightDip = homeUpper.Height, ScrollableHeightDip = ((ScrollViewer)pages[0]).ScrollableHeight }, Backend.JsonOptions));
        await File.WriteAllTextAsync(Path.Combine(directory, "game-folders-layout.json"), JsonSerializer.Serialize(new { Page = "Play", Rows = homeUpper.Children.OfType<FrameworkElement>().Where(c => AutomationProperties.GetAutomationId(c).StartsWith("GameFolder")).Select(c => new { Id = AutomationProperties.GetAutomationId(c), X = Canvas.GetLeft(c), Y = Canvas.GetTop(c), Width = c.ActualWidth, Height = c.ActualHeight }), UpperHeight = homeUpper.Height }, Backend.JsonOptions));
        var handle = new WindowInteropHelper(this).Handle;
        var monitor = new MonitorInfo { Size = Marshal.SizeOf<MonitorInfo>() };
        GetMonitorInfo(MonitorFromWindow(handle, 2), ref monitor); GetWindowRect(handle, out var windowRect);
        var footerBottom = FooterFrame.PointToScreen(new Point(0, FooterFrame.ActualHeight));
        await File.WriteAllTextAsync(Path.Combine(directory, "window-geometry.json"), JsonSerializer.Serialize(new { Maximized = WindowState == WindowState.Maximized, WorkArea = new { monitor.Work.Left, monitor.Work.Top, monitor.Work.Right, monitor.Work.Bottom }, Window = new { windowRect.Left, windowRect.Top, windowRect.Right, windowRect.Bottom }, FooterBottom = footerBottom.Y, FooterInsideWorkArea = footerBottom.Y <= monitor.Work.Bottom }, Backend.JsonOptions));
        ShowPage(0, false); SetStatus("WPF screenshots exported.");
        if (args.Contains("--capture-menu-motion"))
        {
            double[] LabelPositions() => navButtons.Select(button => ((Grid)button.Content).Children[1].TranslatePoint(new Point(), Navigation).Y).ToArray();
            var labelPositions = LabelPositions();
            double start = NavigationSelectionTransform.Y;
            ShowPage(names.Length - 1); await Task.Delay(65);
            double early = NavigationSelectionTransform.Y;
            SaveVisual(Path.Combine(directory, "menu-motion-early.png"));
            await Task.Delay(60); double later = NavigationSelectionTransform.Y;
            SaveVisual(Path.Combine(directory, "menu-motion-later.png"));
            double beforeRetarget = NavigationSelectionTransform.Y;
            ShowPage(1); await Task.Delay(12);
            double afterRetarget = NavigationSelectionTransform.Y;
            ShowPage(3); await Task.Delay(350);
            double final = NavigationSelectionTransform.Y;
            double target = navButtons[3].TranslatePoint(new Point(), Navigation).Y;
            SaveVisual(Path.Combine(directory, "menu-motion-final.png"));
            await File.WriteAllTextAsync(Path.Combine(directory, "menu-motion.json"), JsonSerializer.Serialize(new
            {
                Start = start, Early = early, Later = later, BeforeRetarget = beforeRetarget, AfterRetarget = afterRetarget,
                Final = final, Target = target, LabelPositionsBefore = labelPositions, LabelPositionsAfter = LabelPositions(),
                DurationMs = NavigationSelectionSeconds * 1000, Curve = "Cubic EaseInOut"
            }, Backend.JsonOptions));
            ShowPage(0, false);
        }
        if (args.Contains("--capture-loading") || args.Contains("--capture-loading-frames"))
        {
            BusyOverlay.Visibility = Visibility.Visible;
            ProgressText.Text = "Original loading animation. No game is being prepared or launched.";
            Progress.Value = 45; PercentText.Text = "45%";
            Root.UpdateLayout(); var loading = StartLoadingAnimation();
            if (args.Contains("--capture-loading-frames")) await CaptureStartupFrames(directory, loading);
            else
            {
                await Task.Delay(450); SaveVisual(Path.Combine(directory, "loading-slide.png"));
                await Task.Delay(200); SaveVisual(Path.Combine(directory, "loading-intro.png"));
                await Task.Delay(1050); SaveVisual(Path.Combine(directory, "loading.png"));
            }
            spinnerTimer?.Stop(); BusyOverlay.Visibility = Visibility.Collapsed;
        }
        if (args.Contains("--capture-splash"))
        {
            var previewLaunch = Launch(args.Contains("--capture-benchmark-splash"));
            await Task.Delay(700);
            if (launchSplash == null || !launchSplash.IsVisible || IsVisible)
                throw new InvalidOperationException("The loading view is not a separate visible splash.");
            SaveVisual(Path.Combine(directory, "launch-splash.png"), BusyOverlay);
            var splashInfo = new { Title = launchSplash.Title, launchSplash.Topmost,
                launchSplash.ShowInTaskbar, Width = launchSplash.ActualWidth,
                Height = launchSplash.ActualHeight, MainHidden = !IsVisible };
            await previewLaunch;
            if (launchSplash != null || !IsVisible || busy)
                throw new InvalidOperationException("Preview splash did not restore the launcher.");
            File.WriteAllText(Path.Combine(directory, "launch-splash.json"),
                JsonSerializer.Serialize(new { Splash = splashInfo, Restored = true, GameStarted = false }, Backend.JsonOptions));
        }
        if (args.Contains("--exit")) { allowClose = true; Close(); }
    }
    private void SaveVisual(string file, FrameworkElement? target = null)
    {
        target ??= Root; target.UpdateLayout(); var dpi = VisualTreeHelper.GetDpi(target);
        var bitmap = new RenderTargetBitmap((int)(target.ActualWidth * dpi.DpiScaleX), (int)(target.ActualHeight * dpi.DpiScaleY), 96 * dpi.DpiScaleX, 96 * dpi.DpiScaleY, PixelFormats.Pbgra32);
        bitmap.Render(target); var encoder = new PngBitmapEncoder(); encoder.Frames.Add(BitmapFrame.Create(bitmap)); using var stream = File.Create(file); encoder.Save(stream);
    }
    private void Rendered(object? sender, EventArgs args)
    {
        var time = ((RenderingEventArgs)args).RenderingTime;
        if (lastRender != default && time != lastRender) renderIntervals.Add((time - lastRender).TotalMilliseconds);
        lastRender = time;
    }
    private async Task MeasureAnimations()
    {
        // Warm both visual caches before measuring animation-only frames.
        ShowPage(2, false); await Task.Delay(200); ShowPage(0, false); await Task.Delay(200);
        CompositionTarget.Rendering += Rendered;
        for (int i = 0; i < 24; ++i) { ShowPage(i % 2 == 0 ? 2 : 0); await Task.Delay(350); }
        CompositionTarget.Rendering -= Rendered;
        var sorted = renderIntervals.Where(x => x > 0).Order().ToArray();
        double p(double percentile) => sorted.Length == 0 ? 0 : sorted[Math.Min(sorted.Length - 1, (int)(sorted.Length * percentile))];
        var result = new { Framework = ".NET 8 WPF", HardwareTier = RenderCapability.Tier >> 16, MonitorHz = refreshRate, Samples = sorted.Length, CallbackMedianMs = p(.5), CallbackP95Ms = p(.95), CallbackP99Ms = p(.99), Intervals = renderIntervals, Measurement = "WPF CompositionTarget.Rendering callback intervals; not hardware presentation timestamps", Caption = "Integrated WindowChrome; opaque HWND; square corners", Animation = "280 ms cubic ease-in-out slide: page enter +32 to 0, exit 0 to -12; smoothstep opacity; backgrounds fade without translation" };
        await File.WriteAllTextAsync(Path.Combine(root, "wpf-performance.json"), JsonSerializer.Serialize(result, Backend.JsonOptions));
        ShowPage(0); SetStatus($"WPF tier {result.HardwareTier} / {refreshRate} Hz / callbacks median {p(.5):F2} ms, p95 {p(.95):F2} ms");
        if (Environment.GetCommandLineArgs().Contains("--exit")) { allowClose = true; Close(); }
    }
}

// The exact smoothstep timing curve from the first launcher visual layer.
public sealed class OriginalMenuEase : EasingFunctionBase
{
    public OriginalMenuEase() { EasingMode = EasingMode.EaseIn; }
    protected override double EaseInCore(double t) => t * t * (3 - 2 * t);
    protected override Freezable CreateInstanceCore() => new OriginalMenuEase();
}
