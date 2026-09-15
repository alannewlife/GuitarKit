// main/metronome_audio.h —— 节拍器播放任务: 合成咔嗒声并发布当前拍。
#pragma once

#include <stdint.h>

typedef struct {
    int playing;   // 0=停止, 1=播放中
    int bpm;       // 当前速度(任务侧快照)
    int beat;      // 当前拍 0..3, 0 为小节重音
    int seq;       // 每拍 +1(UI 识别新拍)
} metronome_info_t;

// 启动播放任务(只创建一次)。内部自行 bsp_audio_init(幂等)。
void metronome_audio_start(void);

// 设置运行状态与速度(BPM 越界时只更新 playing)。播放中改速度即时生效。
void metronome_audio_set(int playing, int bpm);

// 读取任务侧快照(单写者单读者,字段可能来自相邻两拍,展示无影响)。
metronome_info_t metronome_audio_info(void);
