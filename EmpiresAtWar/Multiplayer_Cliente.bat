@echo off
rem Entra no servidor. Uso: Multiplayer_Cliente.bat 192.168.0.10  (sem IP = este computador)
set IP=%1
if "%IP%"=="" set IP=127.0.0.1
set UE="C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\Win64\UE4Editor.exe"
start "EAW Cliente" %UE% "%~dp0EmpiresAtWar.uproject" %IP% -game -windowed -ResX=1280 -ResY=720 -WinX=640 -WinY=80 -EAWCampo -log
