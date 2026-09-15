// main/capo_model.h —— 变调夹速查的纯计算(不依赖 ESP-IDF/LVGL,可在主机上测试)。
// 回答初学者最常见的问题: "夹 N 品、按 X 调的指法,实际是什么调?"
#pragma once

// 吉他上常用的可动指法调(不是全部 12 个,是左手好按的那几个)。
#define CAPO_SHAPE_KEY_COUNT 5

typedef struct {
    const char *name;      // "C"
    int pitch_class;       // 0=C, 1=C#, ... 11=B
} capo_key_t;

extern const capo_key_t CAPO_SHAPE_KEYS[CAPO_SHAPE_KEY_COUNT];

// 变调夹范围: 0(不夹) .. 7 品。再高音色变薄,初学者也极少用。
#define CAPO_MAX_FRET 7

// 音级 -> 调名(升号写法),pitch_class 超出 0..11 返回 NULL。
const char *capo_key_name(int pitch_class);

// 夹 fret 品后按 shape_pc 调指法,实际的音级。(shape_pc + fret) 对 12 取模。
int capo_sounding_pc(int shape_pc, int fret);
