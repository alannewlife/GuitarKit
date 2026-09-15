// tests/test_metronome.c —— 节拍器速度换算的主机测试。
#include <assert.h>
#include "metronome.h"

int main(void)
{
    assert(METRO_BEATS == 4);
    assert(metronome_clamp_bpm(100) == 100);
    assert(metronome_clamp_bpm(10) == METRO_BPM_MIN);    // 40
    assert(metronome_clamp_bpm(999) == METRO_BPM_MAX);   // 240

    assert(metronome_interval_samples(120, 16000) == 8000);
    assert(metronome_interval_samples(240, 16000) == 4000);
    assert(metronome_interval_samples(40, 16000) == 24000);
    assert(metronome_interval_samples(0, 16000) == 0);   // 防御: bpm<=0
    return 0;
}
