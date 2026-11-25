@echo off
rem Teste no mesmo PC: abre o servidor e, 8 s depois, um cliente conectado nele.
call "%~dp0Multiplayer_Servidor.bat"
timeout /t 8 /nobreak >nul
call "%~dp0Multiplayer_Cliente.bat" 127.0.0.1
