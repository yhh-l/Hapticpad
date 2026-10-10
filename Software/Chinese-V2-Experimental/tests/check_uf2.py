"""主机侧校验 UF2 的 RP2040 家族、块序号与连续闪存地址；不验证实机行为。"""
from pathlib import Path
import struct
import sys

assert len(sys.argv) == 2, "usage: python3 check_uf2.py <firmware.uf2>"
data = Path(sys.argv[1]).read_bytes()
assert data and len(data) % 512 == 0
blocks = len(data) // 512
last_addr = None
for i in range(blocks):
    b = data[i * 512:(i + 1) * 512]
    magic0, magic1, flags, addr, payload, number, count, family = struct.unpack_from('<8I', b)
    assert (magic0, magic1) == (0x0A324655, 0x9E5D5157)
    assert flags & 0x2000 and payload == 256
    assert number == i and count == blocks
    assert family == 0xE48BFF56, f"not RP2040 family: {family:#x}"
    assert struct.unpack_from('<I', b, 508)[0] == 0x0AB16F30
    if last_addr is None:
        assert addr == 0x10000000
    else:
        assert addr == last_addr + 256
    last_addr = addr
print(f"通过：RP2040 UF2，共 {blocks} 块，连续闪存地址")
