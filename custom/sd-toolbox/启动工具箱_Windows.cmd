@echo off
chcp 65001 >nul
cd /d "%~dp0"
py -3 --version >nul 2>&1
if not errorlevel 1 (
  py -3 server.py --port 8770 --open
) else (
  python --version >nul 2>&1
  if errorlevel 1 (
    echo 请先安装 Python 3.9 或更高版本：https://www.python.org/downloads/
  ) else (
    python server.py --port 8770 --open
  )
)
pause
