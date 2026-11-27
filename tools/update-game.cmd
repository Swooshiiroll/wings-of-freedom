@echo off
rem Installs or updates Wings of Freedom to the newest build on GitHub (see update-game.ps1).
rem Double-click it, or run it with the same options: update-game.cmd -Launch
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0update-game.ps1" %*
if errorlevel 1 pause
