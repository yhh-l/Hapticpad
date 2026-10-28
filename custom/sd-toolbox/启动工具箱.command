#!/bin/zsh
cd -- "$(dirname -- "$0")" || exit 1
if ! command -v python3 >/dev/null; then
  echo '请先安装 Python 3.9 或更高版本：https://www.python.org/downloads/'
  read '?按回车退出'; exit 1
fi
chmod +x tools/picotool 2>/dev/null
python3 server.py --port 8770 --open
