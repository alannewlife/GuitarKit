#!/usr/bin/env bash
# 合并固件: 生成从 0x0 起可整写的完整镜像(引导+分区表+应用),
# 用于提交商店审核。自己设备日常升级请继续用分段 idf.py flash(保护 cardid)。
set -euo pipefail
cd "$(dirname -- "${BASH_SOURCE[0]}")/../firmware"

if ! command -v idf.py >/dev/null 2>&1; then
    source "${IDF_PATH:-$HOME/esp/esp-idf-v5.5.3}/export.sh" >/dev/null
fi

OUT="guitar-kit-full-$(date +%Y%m%d).bin"
idf.py merge-bin -o "$OUT" >/dev/null
ls -la "build/$OUT"

python3 - "$OUT" <<'EOF'
import os, sys
data = open(f"build/{sys.argv[1]}", "rb").read()
ok = data[0] == 0xE9 and data[0x8000:0x8002] == b"\xAA\x50" and len(data) < 0x356000
print("合并镜像校验:", "PASS" if ok else "FAIL")
sys.exit(0 if ok else 1)
EOF
