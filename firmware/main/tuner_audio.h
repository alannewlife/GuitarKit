// main/tuner_audio.h —— 麦克风采音与测频任务的接口。
#pragma once

#include <stdint.h>

// 一次检测结果。freq_x100 为 0 表示当前没有检测到音高。
typedef struct {
    int freq_x100;   // 检测到的频率 x100,如 11025 = 110.25Hz
    int nearest;     // 最近的弦下标(TUNER_STRINGS),无音时为 -1
    int cents;       // 相对最近弦的音分差(+偏高/−偏低)
    int seq;         // 帧序号,每帧 +1(UI 用来识别新数据)
} tuner_result_t;

// 启动后台采音任务(内部完成 bsp_audio_init 与 16kHz 格式设置,只启动一次)。
void tuner_audio_start(void);

// 读取最新结果(单写者单读者,字段可能来自相邻两帧,UI 展示上无影响)。
tuner_result_t tuner_audio_result(void);
