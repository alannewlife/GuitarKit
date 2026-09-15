// main/metronome.c —— 节拍器速度的合法范围与拍间隔换算。
#include "metronome.h"

int metronome_clamp_bpm(int bpm)
{
    if (bpm < METRO_BPM_MIN) return METRO_BPM_MIN;
    if (bpm > METRO_BPM_MAX) return METRO_BPM_MAX;
    return bpm;
}

int metronome_interval_samples(int bpm, int fs)
{
    if (bpm <= 0) return 0;
    return fs * 60 / bpm;
}
