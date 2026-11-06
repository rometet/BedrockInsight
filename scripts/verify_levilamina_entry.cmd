@echo off
setlocal EnableExtensions EnableDelayedExpansion

if "%BEDROCKINSIGHT_CLANG_ROOT%"=="" (
    echo Set BEDROCKINSIGHT_CLANG_ROOT to the clang toolchain bin directory.
    exit /b 2
)
if "%BEDROCKINSIGHT_LEVILAMINA_ROOT%"=="" (
    echo Set BEDROCKINSIGHT_LEVILAMINA_ROOT to the read-only LeviLamina source reference.
    exit /b 2
)
if "%BEDROCKINSIGHT_XMAKE_PACKAGES%"=="" set "BEDROCKINSIGHT_XMAKE_PACKAGES=%LOCALAPPDATA%\.xmake\packages"
if not exist "%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" (
    echo clang++ was not found under %BEDROCKINSIGHT_CLANG_ROOT%.
    exit /b 2
)
if not exist "%BEDROCKINSIGHT_LEVILAMINA_ROOT%\src\ll\api\mod\RegisterHelper.h" (
    echo RegisterHelper.h was not found under %BEDROCKINSIGHT_LEVILAMINA_ROOT%.
    exit /b 2
)
if not exist "%BEDROCKINSIGHT_XMAKE_PACKAGES%" (
    echo Installed xmake package root was not found: %BEDROCKINSIGHT_XMAKE_PACKAGES%
    exit /b 2
)

if not exist build\manual mkdir build\manual
set "INCLUDES=-Isrc -I"%BEDROCKINSIGHT_LEVILAMINA_ROOT%\src" -I"%BEDROCKINSIGHT_LEVILAMINA_ROOT%\src-server""
for /f "delims=" %%D in ('dir /s /b /ad "%BEDROCKINSIGHT_XMAKE_PACKAGES%" ^| findstr /i "\\include$"') do set "INCLUDES=!INCLUDES! -I"%%D""
"%BEDROCKINSIGHT_CLANG_ROOT%\clang++.exe" -std=c++20 -fsyntax-only !INCLUDES! src\bedrock_insight\levilamina\mod.cpp
if errorlevel 1 exit /b %errorlevel%
echo BedrockInsight LeviLamina entry probe: PASS
exit /b 0
