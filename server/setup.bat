@echo off
echo === box-demo ASR Server Setup ===
cd /d "%~dp0"

if not exist ".venv" (
    echo Creating uv virtual environment...
    uv venv --python 3.12
)

echo Installing dependencies...
.venv\Scripts\pip.exe install faster-whisper flask

echo.
echo Setup complete!
echo.
echo Run: run.bat
echo Or: .venv\Scripts\activate ^& python asr_server.py
pause
