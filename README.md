# OpenRGB Run as Administrator

<img src="icon.png" width="64" align="right">

An [OpenRGB](https://openrgb.org) plugin that makes OpenRGB always run elevated, **without a UAC prompt on
every start**. Some sensors only work from an elevated process, e.g. CPU temperatures in the
[Hardware Sync](https://gitlab.com/OpenRGBDevelopers/OpenRGBHardwareSyncPlugin) plugin (read through PawnIO),
which otherwise show `0`.

Works with **OpenRGB 1.1 on Windows x64**.

## How it works

1. The first time OpenRGB starts without admin rights, the plugin asks once whether to set this up. On yes,
   Windows shows one UAC prompt and a scheduled task `OpenRGB - elevated` is created ("run with highest
   privileges", started on demand only).
2. From then on, a non-elevated OpenRGB, for example from the Startup folder, starts that task and exits.
   The elevated copy waits for it to close, then takes over. `--startminimized` is kept; other command-line
   arguments are not.
3. **Crash guard:** the elevated copy marks itself healthy after 15 seconds. If an elevated start ever dies
   before that, later starts stay non-elevated instead of handing off into a crash.

## Install

1. Download `openrgb-elevate-<version>-openrgb-1.1-win64.zip` from [Releases](https://github.com/KrapFey/openrgb-elevate/releases) and unzip it.
2. Exit OpenRGB (tray icon > Exit).
3. In the unzipped folder run:
   ```powershell
   powershell -ExecutionPolicy Bypass -File .\install.ps1
   ```
4. Start OpenRGB normally, answer **Yes**, and approve the UAC prompt.

Manual install: copy `plugins\OpenRGBElevatePlugin.dll` to `%APPDATA%\OpenRGB\plugins\`.

## Reset and uninstall

| To | Do |
|---|---|
| Be asked again after answering No | delete `%APPDATA%\OpenRGB\elevate-declined` |
| Retry after the crash guard switched off the handoff | delete `%APPDATA%\OpenRGB\elevate-attempt` |
| Uninstall | delete the DLL, then run `schtasks /delete /tn "OpenRGB - elevated" /f` as admin |

## Troubleshooting

- The task must start OpenRGB **in OpenRGB's own folder** (the plugin sets this). Started from the scheduled
  task default, `System32`, the elevated copy crashed as Hardware Sync loaded.
- A standard (non-admin) Windows account can't elevate this way. The plugin detects that and stays non-elevated.

## Build from source

Same as [openrgb-skin](../openrgb-skin#build-from-source): Visual Studio 2022, CMake, Qt 6.8.3 `msvc2022_64`,
and OpenRGB headers at the 1.1 commit.

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_PREFIX_PATH=C:\Qt-6.8.3\6.8.3\msvc2022_64 -DOPENRGB_SRC=..\openrgb-src
cmake --build build --config Release --target package
```

## License

GPL-2.0-or-later, the same as OpenRGB, whose headers this plugin builds against. See [LICENSE](LICENSE).
