@echo off
setlocal
cd /d "%~dp0\.."
cmake --preset windows-vs-debug
if errorlevel 1 exit /b %errorlevel%
cmake --build --preset windows-vs-debug
exit /b %errorlevel%
