> [!NOTE]
> This repository is a fork branch.
> The original [upstream project](https://github.com/Aetopia/NoSteamWebHelper.git) is deprecated.

# NoSteamWebHelper
 A program that disables Steam's CEF/Chromium Embedded Framework.

## Aim
This program was created with the intent of replacing of Steam's command-line parameter `-no-browser` which was [removed.](https://steamcommunity.com/groups/SteamClientBeta/discussions/3/3710433479207750727/?ctp=42)


## How does NoSteamWebHelper kill or disable the CEF/Chromium Embedded Framework?

The dynamic link library toggles the CEF depending if an is app running or not.

- If an app is running then the CEF is disabled.

- If an app is not running then the CEF is enabled.

This way, Steam is still accessible to use.

# Usage
## Installer
1. Build the installer with `BuildInstaller.cmd`.

2. Make sure Steam is fully closed.

3. Run `installer\bin\NoSteamWebHelperSetup.exe`.

4. The installer detects the Steam directory, backs up an existing `umpdc.dll` to `umpdc.dll.bak`, and installs the new DLL.

5. Use the generated uninstaller to remove `umpdc.dll` and restore the backup when available.

## Manual install
1. Download the latest release from [GitHub Releases](https://github.com/Aetopia/NoSteamWebHelper/releases).

2. Place `umpdc.dll` in your Steam installation directory where `steam.exe` is located.

3. Make sure Steam is fully closed and launch a new instance of Steam.

4. Start up an app and the CEF will be toggled accordingly.

> [!NOTE]
> - You may manually toggle the CEF from the tray icon.
> - Right-click the tray icon to enable or disable CEF.
> - To prevent the CEF from automatically showing when restored, pass `-silent` to Steam.

## Build
1. Install & update [MSYS2](https://www.msys2.org):

    ```bash
    pacman -Syu --noconfirm
    ```

3. Install [GCC](https://gcc.gnu.org):

    ```bash
    pacman -Syu mingw-w64-ucrt-x86_64-gcc --noconfirm
    ```


3. Start MSYS2's `UCRT64` environment & run `Build.cmd`.

4. To build the optional installer, install [Inno Setup](https://jrsoftware.org/isinfo.php) and run `BuildInstaller.cmd`.
