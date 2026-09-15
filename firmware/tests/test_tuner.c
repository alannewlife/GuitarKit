// tests/test_tuner.c —— 调音算法的主机测试: 合成单音 -> 检测频率应收敛到真值。
// 覆盖: 六根弦的目标频率表、静音门限、直流偏置鲁棒性、倍周期校正、插值精度、音分换算。
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "tuner.h"

#define FS 16000
#define N  1024

// 生成带二次谐波与少量噪声的正弦(比纯音更接近真实拾音, 且考验倍周期校正)。
static int make_tone(int16_t *x, int n, double hz, double amp, double bias)
{
    double phase = 0.0;
    unsigned seed = 12345;
    for (int i = 0; i < n; i++) {
        phase += 2.0 * M_PI * hz / FS;
        if (phase > 2.0 * M_PI) phase -= 2.0 * M_PI;
        double v = amp * sin(phase) + 0.4 * amp * sin(2.0 * phase);
        seed = seed * 1103515245u + 12345u;          // 小幅确定性噪声
        v += ((int)(seed >> 16) % 200 - 100) / 32768.0 * amp;
        x[i] = (int16_t)(v + bias);
    }
    return 0;
}

static void expect_freq(int16_t *x, double hz)
{
    int f = tuner_detect_freq_x100(x, N, FS);
    double cents = tuner_cents((int)(hz * 100), f);
    printf("expect %.2fHz -> %d.%02dHz (%+d cent)\n",
           hz, f / 100, f % 100, tuner_cents((int)(hz * 100), f));
    assert(f > 0);
    assert(cents > -8 && cents < 8);
}

int main(void)
{
    /* ---- 弦表 ---- */
    assert(TUNER_STRING_COUNT == 6);
    assert(strcmp(TUNER_STRINGS[0].name, "E") == 0);
    assert(TUNER_STRINGS[0].freq_x100 == 8241);
    assert(TUNER_STRINGS[5].freq_x100 == 32963);
    assert(tuner_nearest_string(11050) == 1);      // 110.5Hz 离 A2 最近
    assert(tuner_nearest_string(0) == -1);

    /* ---- 音分换算 ---- */
    assert(tuner_cents(11000, 11000) == 0);
    assert(tuner_cents(11000, 22000) == 1200);     // 高一个八度
    int c = tuner_cents(11000, 11654);             // 高半音附近(A2->A#2 ≈ +100)
    assert(c >= 95 && c <= 100);

    /* ---- 静音门限: 直流偏置但没有交流成分 ---- */
    int16_t x[N];
    for (int i = 0; i < N; i++) x[i] = 2000;
    assert(tuner_detect_freq_x100(x, N, FS) == 0);
    assert(tuner_rms(x, N) == 0);                  // 去直流后 RMS 为 0

    /* ---- 六根弦逐一检测(带谐波/噪声/直流偏置) ---- */
    make_tone(x, N, 82.41,  12000, 3000);  expect_freq(x, 82.41);
    make_tone(x, N, 110.00, 12000, -3000); expect_freq(x, 110.00);
    make_tone(x, N, 146.83, 10000, 0);     expect_freq(x, 146.83);
    make_tone(x, N, 196.00, 10000, 1500);  expect_freq(x, 196.00);
    make_tone(x, N, 246.94, 9000,  0);     expect_freq(x, 246.94);
    make_tone(x, N, 329.63, 8000,  -2500); expect_freq(x, 329.63);

    /* ---- 偏低的音: 音分应为负 ---- */
    make_tone(x, N, 108.0, 12000, 0);
    int f = tuner_detect_freq_x100(x, N, FS);
    assert(tuner_cents(11000, f) < -10);

    /* ---- 锁弦窄窗检测: 高音弦失谐时误差应 <=3 音分(细化后) ---- */
    struct { double hz; int center; double detuned; } near_cases[] = {
        { 196.00,   19600, 203.9  },   // G3 偏高 ~+67 音分
        { 246.94,   24694, 240.0  },   // B3 偏低 ~-49 音分
        { 329.63,   32963, 341.0  },   // E4 偏高 ~+58 音分
        { 329.63,   32963, 318.0  },   // E4 偏低 ~-62 音分
    };
    for (unsigned i = 0; i < sizeof(near_cases)/sizeof(near_cases[0]); i++) {
        make_tone(x, N, near_cases[i].detuned, 10000, 500);
        int fn = tuner_detect_freq_x100_near(x, N, FS, near_cases[i].center);
        int cn = tuner_cents((int)(near_cases[i].detuned * 100), fn);
        printf("near %.2fHz center %d -> %d.%02dHz (%+d cent)\n",
               near_cases[i].detuned, near_cases[i].center,
               fn / 100, fn % 100, cn);
        assert(fn > 0);
        assert(cn > -3 && cn < 3);
    }

    /* ---- 窗口拒收: 锁 E4 却弹 A2(差约 -1800 音分) -> 无效 ---- */
    make_tone(x, N, 110.0, 12000, 0);
    assert(tuner_detect_freq_x100_near(x, N, FS, 32963) == 0);

    /* ---- 静音/响度不足: 锁弦模式同样返回 0 ---- */
    for (int i = 0; i < N; i++) x[i] = 100;
    assert(tuner_detect_freq_x100_near(x, N, FS, 19600) == 0);

    return 0;
}
