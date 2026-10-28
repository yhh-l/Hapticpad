"""Numeric parser regressions now run against the SD-backed production parser."""
import runpy
from pathlib import Path
runpy.run_path(str(Path(__file__).with_name("check_sd_storage.py")), run_name="__main__")
