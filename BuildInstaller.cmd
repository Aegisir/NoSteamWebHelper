@echo off
setlocal

cd /d "%~dp0"

call "%~dp0Build.cmd"
if errorlevel 1 exit /b 1

set "ISCC="
for /f "delims=" %%I in ('where ISCC.exe 2^>nul') do (
    set "ISCC=%%I"
    goto :found_iscc
)

if exist "%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe" (
    set "ISCC=%ProgramFiles(x86)%\Inno Setup 6\ISCC.exe"
    goto :found_iscc
)

if exist "%ProgramFiles%\Inno Setup 6\ISCC.exe" (
    set "ISCC=%ProgramFiles%\Inno Setup 6\ISCC.exe"
    goto :found_iscc
)

if exist "%LocalAppData%\Programs\Inno Setup 6\ISCC.exe" (
    set "ISCC=%LocalAppData%\Programs\Inno Setup 6\ISCC.exe"
    goto :found_iscc
)

echo Inno Setup compiler ISCC.exe was not found.
echo Install Inno Setup 6 or add ISCC.exe to PATH.
exit /b 1

:found_iscc
"%ISCC%" "%~dp0installer\NoSteamWebHelper.iss"
exit /b %ERRORLEVEL%
