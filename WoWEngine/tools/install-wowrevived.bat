@echo off
rem Copies WoWRevived's upscaled movies into the newest WoWEngine release in out\ and runs the engine, then
rem shows the movie lines of its log (WoWEngine.log, next to WoWEngine.exe) so they can be copied as text.
rem
rem The movies come from WoWRevived's build by default; give another WoWRevived folder as the first argument
rem (install-wowrevived.bat "D:\Games\WoWRevived") or set WOWREVIVED to it.
setlocal enabledelayedexpansion
set "ROOT=%~dp0..\.."
set "SOURCE=%~1"
if "%SOURCE%"=="" set "SOURCE=%WOWREVIVED%"
if "%SOURCE%"=="" set "SOURCE=%ROOT%\WoWLauncher\bin\Debug\WoWRevived-English-v1.1.2"
if not exist "%SOURCE%\FMV" (
    echo WoWRevived's movies weren't found in "%SOURCE%\FMV".
    echo Give WoWRevived's folder as the first argument, or set WOWREVIVED to it.
    goto end
)

rem the newest release folder (not the -debug one)
set "TARGET="
for /f "delims=" %%d in ('dir /b /ad /o-d "%~dp0..\out\WoWEngine-v*" 2^>nul ^| findstr /v /i "debug"') do (
    if not defined TARGET set "TARGET=%~dp0..\out\%%d"
)
if not defined TARGET (
    echo No release in out\. Run make_release.bat first.
    goto end
)

echo Copying WoWRevived's movies from "%SOURCE%"
echo                             into "%TARGET%"
for %%f in (FMV FMV-Human FMV-Martian) do (
    if exist "%SOURCE%\%%f\*.mp4" (
        if not exist "%TARGET%\%%f" mkdir "%TARGET%\%%f"
        copy /y "%SOURCE%\%%f\*.mp4" "%TARGET%\%%f\" >nul && echo   %%f done
    )
)

echo Running the engine...
start "" /wait "%TARGET%\WoWEngine.exe"

echo.
echo The movie lines from WoWEngine.log:
findstr /c:"Movie" /c:"Screen" "%TARGET%\WoWEngine.log"
:end
echo.
pause
