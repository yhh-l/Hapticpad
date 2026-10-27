"""Reproducible local checks only; never contacts or programs a device."""
from pathlib import Path
import subprocess
import sys
import tempfile
import os
ROOT=Path(__file__).resolve().parents[1]
API=Path(os.environ.get('ARDUINO_API',str(Path.home()/'Library/Arduino15/packages/rp2040/hardware/rp2040/6.3.1/ArduinoCore-API/api')))
def run(cmd): subprocess.run(cmd,cwd=ROOT,check=True)
run([sys.executable,'-m','unittest','discover','-s','tests','-p','test_*.py'])
run(['node','--check','web/app.js'])
run(['node','--test','tests/test_shortcuts.cjs'])
with tempfile.TemporaryDirectory(prefix='haptic-flywheel-checks-') as temp:
    for source in sorted((ROOT/'tests').glob('check_*.cpp')):
        output=str(Path(temp)/source.stem)
        run(['clang++','-std=c++17','-Wall','-Wextra','-Werror','-I','firmware/MacroPad',str(source),'-o',output])
        run([output])
if not API.exists():raise SystemExit('缺少已配置的Arduino API测试依赖，请调整verify_local.py中的API路径')
run([sys.executable,'tests/check_firmware.py',str(API)])
run([sys.executable,'tests/check_buttons.py'])
run([sys.executable,'tests/check_menu_storage.py',str(API)])
run(['node','tests/verify_files.cjs'])
print('PASS: all local checks; no device operation performed',flush=True)
