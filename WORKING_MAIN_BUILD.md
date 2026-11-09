# Build the working-main version

The Windows working-main profile reproduces the stable local build foundation from source. It includes the accepted three-mode Sun filtering and preset mapping, authored helper generation, qualified native camera fragment loading, the matching launcher and dependency packaging. High and Ultra share the farther texel-floor correction while near softness and atlas choices remain unchanged.

From the repository root in PowerShell:

```powershell
profiles/working-main/build.ps1 -Checkout . -OutputDirectory D:/Builds/driveclub-working-main-source
profiles/working-main/package.ps1 -SourceDirectory D:/Builds/driveclub-working-main-source -OutputDirectory D:/Builds/driveclub-working-main
```

Use new output directories. The scripts build a separate package; settings and saves can then be copied into it. See [requirements and build options](profiles/working-main/README.md). User-owned game files are prepared at launch through the existing flow.
