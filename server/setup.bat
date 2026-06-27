@echo off
echo === box-demo ASR Server Setup ===
cd /d "%~dp0"

if not exist ".venv" (
    echo Creating uv virtual environment...
    uv venv --python 3.12
)

echo Installing dependencies (may take a while)...
set UV_HTTP_TIMEOUT=300
uv pip install faster-whisper zhconv requests --python .venv\Scripts\python.exe

echo.
echo Setup complete!
echo.
echo Run: run.bat
echo Or: .venv\Scripts\activate ^& python asr_server.py
pause
