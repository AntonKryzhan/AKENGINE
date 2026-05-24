@echo off
setlocal
cd /d "%~dp0\.."
set AK_EDITOR=build\windows-vs-debug\bin\Debug\ak_editor.exe
if not exist "%AK_EDITOR%" (
    echo ak_editor.exe not found. Build first: scripts\build_windows_vs_debug.bat
    pause
    exit /b 1
)
"%AK_EDITOR%"
