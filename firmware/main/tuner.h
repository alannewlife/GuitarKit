// main/tuner.h —— 吉他调音的纯算法(不依赖 ESP-IDF/LVGL,可在主机上测试)。
// 标准调弦 E2 A2 D3 G3 B3 E4,以及"一段 PCM -> 基频 -> 音分偏差"的检测链。
#pragma once

#include <stdint.h>

#define TUNER_STRING_COUNT 6

// 一根弦: 显示名 / 完整名 / 目标频率(单位 0.01Hz)。A4=440Hz 标准调弦。
typedef struct {
    const char *name;     // "E"
    const char *full;     // "E2"
    int freq_x100;        // 8241 = 82.41 Hz
} tuner_string_t;

extern const tuner_string_t TUNER_STRINGS[TUNER_STRING_COUNT];

// 采样段的响度(去直流后的 RMS)。低于 BSP 麦克风底噪即视为没有弹弦。
int tuner_rms(const int16_t *x, int n);

// 锁弦模式: 只在 center 预期滞后 ±580 音分的窄窗内找基频(排除倍/半周期),
// 峰值处再做 1/4 采样步长的分数滞后细化, 把高频弦(周期短)的量化误差压到 ~2 音分。
// 窗内找不到可信峰(没弹/弹错弦/噪声)返回 0。
int tuner_detect_freq_x100_near(const int16_t *x, int n, int fs, int center_x100);

// 从一段 PCM 里估计基频,返回频率 x100(如 11025 = 110.25Hz)。
// 检测不到音(静音/噪声)返回 0。算法: 去直流 -> 归一化自相关 -> 半周期校正
// -> 抛物线插值。要求 n >= 2 * fs / 60(至少两个最低目标周期的窗口)。
int tuner_detect_freq_x100(const int16_t *x, int n, int fs);

// 频率相对目标的音分差(cent): +50=偏高半音的一半, 0=正好。
int tuner_cents(int target_x100, int freq_x100);

// 离 freq 最近的弦下标(-1 表示越界)。
int tuner_nearest_string(int freq_x100);
