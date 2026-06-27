@echo off
cd /d "%~dp0"
"%~dp0.venv\Scripts\python.exe" "%~dp0asr_server.py"
pause
