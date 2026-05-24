@echo off
setlocal
cd /d "%~dp0\.."
if exist build rmdir /s /q build
echo build directory removed
