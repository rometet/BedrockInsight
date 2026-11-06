@echo off
rem All SDK inputs are read-only; generated files stay under this project.
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build_levilamina.ps1" %*
exit /b %errorlevel%
