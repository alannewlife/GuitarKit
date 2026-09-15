// main/metronome.h —— 节拍器的纯计算(不依赖 ESP-IDF/LVGL,可在主机上测试)。
#define METRO_BPM_MIN 40
#define METRO_BPM_MAX 240
#define METRO_BEATS   4          // 固定 4/4 拍

// 限制到 [METRO_BPM_MIN, METRO_BPM_MAX]。
int metronome_clamp_bpm(int bpm);

// 该速度下每拍多少个采样(16kHz 时 120BPM = 8000)。
int metronome_interval_samples(int bpm, int fs);
