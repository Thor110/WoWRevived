@echo off
rem Makes WoWEngine's release (tools\make_release.py) with everything by default: the version from
rem CMakeLists.txt, the Release build in out\build\x64-Release (built first if it isn't there), the files
rem in out\WoWEngine-v<version> and out\WoWEngine-v<version>.zip. Double-click it, or run it with the
rem script's options (make_release.bat --build, make_release.bat --seven-zip, say). The window stays open at
rem the end so the result can be read.
rem
rem Python is found through the py launcher first: python.org's installers put it in place, and on
rem Windows 10 and 11 a bare "python" can be the Microsoft Store's stand-in instead of Python.
cd /d "%~dp0.."
py -3 --version >nul 2>&1
if not errorlevel 1 (
    py -3 "%~dp0make_release.py" %*
    goto done
)
python -c "import sys" >nul 2>&1
if not errorlevel 1 (
    python "%~dp0make_release.py" %*
    goto done
)
echo Python 3 was not found. Install it from python.org (it adds the py launcher), or turn off
echo the "python.exe" App execution alias in Settings, Apps, Advanced app settings.
:done
echo.

pause
