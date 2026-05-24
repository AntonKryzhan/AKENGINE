@echo off
setlocal
cd /d "%~dp0\.."
cmake --preset windows-ninja-debug
if errorlevel 1 exit /b %errorlevel%
cmake --build --preset windows-ninja-debug
exit /b %errorlevel%
