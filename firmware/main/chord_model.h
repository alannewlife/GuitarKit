// main/chord_model.h —— 吉他和弦词典的纯数据模型(不依赖 ESP-IDF/LVGL,可在主机上测试)。
// 内容: C 大调与 G 大调的 7 个级数, 每级 2~3 个变体(调内三和弦 + 调内七和弦 + 跨性质七和弦)。
// 拓展方式: 给某一级的变体数组末尾追加一条(如 sus4/add9/6), 上层 UI 与按键语义不变。
#pragma once

#include <stdint.h>

#define CHORD_KEY_COUNT     2
#define CHORD_DEGREE_COUNT  7
#define CHORD_STRINGS       6
#define CHORD_MAX_VARIANTS  3

// 一个可按的具体指法。fret/finger 都按 6 弦(低音 E)到 1 弦(高音 e)排列。
typedef struct {
    const char *name;     // "Cmaj7"
    const char *quality;  // "Maj 7"(界面只用 ASCII, 固件未内嵌中文字库)
    const char *notes;    // "G B D F#"(构成音)
    const char *hint;     // 按法提示, 如 "Barre"; 无提示为 ""
    int8_t fret[CHORD_STRINGS];   // -1=闷音, 0=空弦, 1..12=品位(数据保证 <=4)
    int8_t finger[CHORD_STRINGS]; // 0=不按, 1..4=食中无小
} chord_variant_t;

// 调内一个级数(I..vii°)及其变体表。
typedef struct {
    const char *roman;    // "V"
    const char *func;     // "Dominant"
    const chord_variant_t *variants;
    uint8_t variant_count;
} chord_degree_t;

// 一个调(C 大调 / G 大调)。
typedef struct {
    const char *name;     // "C Major"
    const chord_degree_t *degrees;  // 共 CHORD_DEGREE_COUNT 项
} chord_key_t;

extern const chord_key_t CHORD_KEYS[CHORD_KEY_COUNT];

// 带越界保护的访问器: key/degree/variant 超出范围时返回 NULL。
const chord_variant_t *chord_variant_get(int key, int degree, int variant);
// 越界时返回 0。
int chord_variant_count(int key, int degree);
