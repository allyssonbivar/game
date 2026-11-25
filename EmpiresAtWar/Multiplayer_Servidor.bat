@echo off
rem Abre o jogo como servidor (voce joga e os outros entram). Porta 7777 (UDP).
set UE="C:\Program Files\Epic Games\UE_4.27\Engine\Binaries\Win64\UE4Editor.exe"
start "EAW Servidor" %UE% "%~dp0EmpiresAtWar.uproject" /Engine/Maps/Templates/Template_Default?listen -game -windowed -ResX=1280 -ResY=720 -WinX=0 -WinY=40 -EAWCampo -log
