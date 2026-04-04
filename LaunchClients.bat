@echo off
:: ============================================================
::  LaunchClients.bat
::  Launches two standalone game clients for multiplayer testing.
::  No PIE artifacts, no shared worlds, no WorldDataLayers conflicts.
::
::  Usage:
::    LaunchClients.bat          — launch 2 clients (default)
::    LaunchClients.bat 3        — launch N clients
:: ============================================================

setlocal

:: --- Configuration -----------------------------------------------------------
set UE_EDITOR=D:\Game Dev\UE\UE_5.7\Engine\Binaries\Win64\UnrealEditor.exe
set UPROJECT=%~dp0Prototyping.uproject
set MAP=/Game/Maps/WorldMapV1
set COUNT=%~1
if "%COUNT%"=="" set COUNT=2

:: Window size for each client (side by side on 1920-wide monitor)
set WIN_W=960
set WIN_H=540

:: Startup X positions for each client window
set X0=0
set X1=960
set X2=0
set X3=960
:: -----------------------------------------------------------------------------

echo Launching %COUNT% standalone client(s)...
echo UE:       %UE_EDITOR%
echo Project:  %UPROJECT%
echo Map:      %MAP%
echo.

for /L %%i in (1,1,%COUNT%) do (
    set /A IDX=%%i-1
    call :launch %%i !IDX!
    timeout /t 1 /nobreak >nul
)

echo All clients launched.
goto :eof

:launch
set CLIENT_NUM=%1
set IDX=%2

:: Pick window X position (cycles through 4 slots)
set POSITIONS=%X0% %X1% %X2% %X3%
for /F "tokens=%CLIENT_NUM%" %%p in ("%POSITIONS%") do set WIN_X=%%p

echo Starting client %CLIENT_NUM% at x=%WIN_X%...

start "Client %CLIENT_NUM%" "%UE_EDITOR%" "%UPROJECT%" %MAP% ^
    -game ^
    -windowed ^
    -ResX=%WIN_W% ^
    -ResY=%WIN_H% ^
    -WinX=%WIN_X% ^
    -WinY=0 ^
    -NoSplash ^
    -NoVerifyGC ^
    -log ^
    -PIEVIACONSOLE

goto :eof
