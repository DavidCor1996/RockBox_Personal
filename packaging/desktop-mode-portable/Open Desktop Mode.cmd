@echo off
setlocal
set "VOLUME_ROOT=%~dp0"
set "BUNDLE=%VOLUME_ROOT%.rockbox\desktop-host\windows-x86_64"
set "RUNTIME=%BUNDLE%\rockboxui.exe"
set "SYSTEM_ROOT=%BUNDLE%\system-root"

if not exist "%RUNTIME%" goto missing
if not exist "%SYSTEM_ROOT%\.rockbox\rocks\apps\desktop_mode.rock" goto missing

set "RBROOT=%VOLUME_ROOT%"
set "ROCKBOX_SIM_SYSTEM_ROOT=%SYSTEM_ROOT%"
set "ROCKBOX_SIM_PLUGIN=/.rockbox/rocks/apps/desktop_mode.rock"
set "ROCKBOX_SIM_PLUGIN_EXIT=1"
set "ROCKPOD_SIM_HOST_POINTER=%VOLUME_ROOT%.rockbox\host-pointer"
set "PATH=%BUNDLE%;%PATH%"

pushd "%BUNDLE%"
start "" /wait "%RUNTIME%" --fullscreen --nobackground --root "%VOLUME_ROOT%"
set "RESULT=%ERRORLEVEL%"
popd
exit /b %RESULT%

:missing
echo.
echo Desktop Mode portable runtime is missing or incomplete.
echo Reinstall this Rockbox package on the iPod.
echo.
pause
exit /b 1
