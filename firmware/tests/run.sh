#!/usr/bin/env bash
# 独立主机测试: 不依赖 ESP-IDF/LVGL, 用本机 cc 编译运行。
set -euo pipefail
cd "$(dirname -- "${BASH_SOURCE[0]}")/.."

test_dir="$(mktemp -d /tmp/guitar-chords-tests.XXXXXX)"
trap 'rm -rf "${test_dir}"' EXIT

"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_chord_model.c main/chord_model.c \
    -o "${test_dir}/test_chord_model"
"${test_dir}/test_chord_model"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_tuner.c main/tuner.c \
    -o "${test_dir}/test_tuner" -lm
"${test_dir}/test_tuner"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_capo_model.c main/capo_model.c \
    -o "${test_dir}/test_capo_model"
"${test_dir}/test_capo_model"
"${CC:-cc}" -std=c11 -Wall -Wextra -Werror -Imain \
    tests/test_metronome.c main/metronome.c \
    -o "${test_dir}/test_metronome"
"${test_dir}/test_metronome"
echo "Host tests: PASS"
