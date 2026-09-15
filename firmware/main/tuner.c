// main/tuner.c —— 拾音测频: 归一化自相关(NCF)+ 抛物线插值。
// 频率精度: 16kHz 采样下整数滞后在低音区有 +-10 音分级误差,插值后收敛到 +-3 音分内,
// 足够调弦。检测范围 60~500Hz,覆盖六根弦并容许一点按弦偏差。
#include "tuner.h"

#include <math.h>
#include <string.h>

#define TUNER_MIN_HZ   60
#define TUNER_MAX_HZ  500
#define TUNER_RMS_GATE 600     // 16bit PCM 的响度门限: 高于说话/环境声, 轻拨弦可达数千
#define NCF_PEAK_MIN   650     // x1000: 峰值至少 0.65 才算"有音高"(语音自相关置信度低)
#define NCF_OCTAVE     850     // x1000: 半周期处 NCF 达峰值的 0.85 则取半周期

const tuner_string_t TUNER_STRINGS[TUNER_STRING_COUNT] = {
    { "E", "E2",  8241 },
    { "A", "A2", 11000 },
    { "D", "D3", 14683 },
    { "G", "G3", 19600 },
    { "B", "B3", 24694 },
    { "e", "E4", 32963 },
};

// 去直流后的均方根。麦克风信号常带直流偏置,先减均值再算能量。
int tuner_rms(const int16_t *x, int n)
{
    if (n <= 0) return 0;
    int64_t sum = 0;
    for (int i = 0; i < n; i++) sum += x[i];
    int mean = (int)(sum / n);

    int64_t acc = 0;
    for (int i = 0; i < n; i++) {
        int64_t d = x[i] - mean;
        acc += d * d;
    }
    return (int)sqrt((double)acc / n);
}

int tuner_detect_freq_x100(const int16_t *x, int n, int fs)
{
    if (fs <= 0 || n < 2 * fs / TUNER_MIN_HZ) return 0;
    if (tuner_rms(x, n) < TUNER_RMS_GATE) return 0;

    int64_t sum = 0;
    for (int i = 0; i < n; i++) sum += x[i];
    const int mean = (int)(sum / n);

    int lag_min = fs / TUNER_MAX_HZ;
    int lag_max = fs / TUNER_MIN_HZ;
    if (lag_max >= n) lag_max = n - 1;

    // r0 = 去直流信号的全窗能量。
    double r0 = 0.0;
    for (int i = 0; i < n; i++) {
        double d = x[i] - mean;
        r0 += d * d;
    }
    if (r0 <= 0.0) return 0;

    // NCF(m) = r(m) / sqrt(e0(m) * em(m)),对幅度变化稳健,取值 -1..1。
    // e0(m)/em(m) 是窗口与滞后段的各自能量。
    int best = 0;
    double best_ncf = 0.0;
    double ncf[fs / TUNER_MIN_HZ + 1];
    memset(ncf, 0, sizeof(ncf));

    for (int m = lag_min; m <= lag_max; m++) {
        double r = 0.0, e0 = 0.0, em = 0.0;
        for (int i = 0; i < n - m; i++) {
            double a = x[i] - mean, b = x[i + m] - mean;
            r  += a * b;
            e0 += a * a;
            em += b * b;
        }
        ncf[m] = (e0 > 0.0 && em > 0.0)
            ? r / sqrt(e0 * em) : 0.0;
        if (ncf[m] > best_ncf) {
            best_ncf = ncf[m];
            best = m;
        }
    }
    if (best == 0 || best_ncf * 1000.0 < NCF_PEAK_MIN) return 0;

    // 周期校正: 周期信号的 ACF 在 T,2T,3T... 处都有接近峰值的峰,全局峰可能
    // 落在倍周期上(196Hz 被读成 65Hz 即此误)。从短滞后起找第一个达到峰值 85%
    // 的局部峰 —— 最小周期才是基音周期。
    int period = best;
    double thresh = best_ncf * (NCF_OCTAVE / 1000.0);
    for (int m = lag_min; m <= lag_max; m++) {
        if (ncf[m] < thresh) continue;
        double left  = (m > lag_min) ? ncf[m - 1] : -2.0;
        double right = (m < lag_max) ? ncf[m + 1] : -2.0;
        if (ncf[m] >= left && ncf[m] >= right) { period = m; break; }
    }

    // 抛物线插值 sub-sample: 用峰点及其两点作二次拟合,顶点即更精确的滞后。
    double lag = period;
    if (period > lag_min && period < lag_max) {
        double a = ncf[period - 1], b = ncf[period], c = ncf[period + 1];
        double denom = a - 2.0 * b + c;
        if (denom < 0.0) lag = period + 0.5 * (a - c) / denom;
    }

    int freq_x100 = (int)(fs * 100.0 / lag + 0.5);
    if (freq_x100 < TUNER_MIN_HZ * 100 || freq_x100 > TUNER_MAX_HZ * 100) {
        return 0;
    }
    return freq_x100;
}

// 分数滞后的归一化互相关: 滞后段按线性插值取值。
static double ncf_frac(const int16_t *x, int n, int mean, double tau)
{
    int m = (int)tau;
    double frac = tau - m;
    if (m + 1 >= n) return 0.0;
    double r = 0.0, e0 = 0.0, e1 = 0.0;
    for (int i = 0; i + m + 1 < n; i++) {
        double a = x[i] - mean;
        double b = (x[i + m] - mean) * (1.0 - frac)
                 + (x[i + m + 1] - mean) * frac;
        r += a * b;
        e0 += a * a;
        e1 += b * b;
    }
    if (e0 <= 0.0 || e1 <= 0.0) return 0.0;
    return r / sqrt(e0 * e1);
}

int tuner_detect_freq_x100_near(const int16_t *x, int n, int fs, int center_x100)
{
    if (center_x100 < TUNER_MIN_HZ * 100 || center_x100 > TUNER_MAX_HZ * 100) {
        return 0;
    }
    if (fs <= 0 || n < 2 * fs / TUNER_MIN_HZ) return 0;
    if (tuner_rms(x, n) < TUNER_RMS_GATE) return 0;

    int64_t sum = 0;
    for (int i = 0; i < n; i++) sum += x[i];
    const int mean = (int)(sum / n);

    // 窗口: center 周期 ±580 音分(滞后因子约 1.4)。锁弦后倍/半周期必在窗外。
    int lag0 = (int)(fs * 100.0 / center_x100 + 0.5);
    int lag_lo = (int)(lag0 / 1.4);
    int lag_hi = (int)(lag0 * 1.4);
    if (lag_lo < 4) lag_lo = 4;
    if (lag_hi >= n - 2) lag_hi = n - 2;
    if (lag_lo >= lag_hi) return 0;

    double best = 0.0;
    int best_m = 0;
    for (int m = lag_lo; m <= lag_hi; m++) {
        double v = ncf_frac(x, n, mean, (double)m);   // 整数滞后=插值退化为直取
        if (v > best) { best = v; best_m = m; }
    }
    if (best_m == 0 || best * 1000.0 < NCF_PEAK_MIN) return 0;

    // 细化: 峰值 ±0.75 滞后内按 1/4 步扫描, 再对三点抛物线插值。
    double taus[7], vals[7];
    for (int k = 0; k < 7; k++) {
        taus[k] = best_m - 0.75 + 0.25 * k;
        vals[k] = ncf_frac(x, n, mean, taus[k]);
    }
    int top = 0;
    for (int k = 1; k < 7; k++) if (vals[k] > vals[top]) top = k;
    double tau = taus[top];
    if (top > 0 && top < 6) {
        double a = vals[top - 1], b = vals[top], c = vals[top + 1];
        double denom = a - 2.0 * b + c;
        if (denom < 0.0) tau = taus[top] + 0.25 * 0.5 * (a - c) / denom;
    }

    int freq_x100 = (int)(fs * 100.0 / tau + 0.5);
    // 越出窗口对应的音分范围说明不是这根弦的音, 视为无效。
    int cents = tuner_cents(center_x100, freq_x100);
    if (cents < -600 || cents > 600) return 0;
    return freq_x100;
}

int tuner_cents(int target_x100, int freq_x100)
{
    if (target_x100 <= 0 || freq_x100 <= 0) return 0;
    return (int)(1200.0 * log2((double)freq_x100 / target_x100) + 0.5);
}

int tuner_nearest_string(int freq_x100)
{
    if (freq_x100 <= 0) return -1;
    int best_i = -1, best_c = 1 << 30;
    for (int i = 0; i < TUNER_STRING_COUNT; i++) {
        int c = tuner_cents(TUNER_STRINGS[i].freq_x100, freq_x100);
        if (c < 0) c = -c;
        if (c < best_c) { best_c = c; best_i = i; }
    }
    return best_i;
}
