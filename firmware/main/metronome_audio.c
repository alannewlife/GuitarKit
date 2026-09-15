// main/metronome_audio.c —— 节拍器播放任务。
// 咔嗒声由正弦短脉冲合成(小节头 1300Hz 重音, 其余 900Hz, 指数衰减),
// 每拍写一次点击 + 剩余间隔的静音; bsp_audio_write 阻塞在 DMA 满时自然完成节拍 pacing。
// 与调音器共用 ES8311: 全双工, 读(麦克风)与写(扬声器)互不干扰;
// 两个任务都用 16kHz/16bit/单声道, set_format 同值重复调用是廉价的。
#include "metronome_audio.h"
#include "metronome.h"
#include "bsp_audio.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include <string.h>
#include <math.h>

#define METRO_FS        16000
#define CLICK_SAMPLES   800     // 50ms 的咔嗒声
#define TAG "metronome"

static volatile int s_running;
static volatile int s_bpm = 100;
static metronome_info_t s_pub;
static TaskHandle_t s_task;

// 合成的两段点击与静音块,放静态区(共约 2.5KB,不占任务栈)。
static int16_t s_accent[CLICK_SAMPLES];
static int16_t s_tick[CLICK_SAMPLES];
static int16_t s_silence[512];

static void click_buffers_init(void)
{
    for (int i = 0; i < CLICK_SAMPLES; i++) {
        double t = (double)i / METRO_FS;
        double env = exp(-28.0 * t);                       // 50ms 内指数衰减
        s_accent[i] = (int16_t)(9000.0 * env * sin(2.0 * M_PI * 1300.0 * t));
        s_tick[i]   = (int16_t)(6000.0 * env * sin(2.0 * M_PI * 900.0 * t));
    }
    memset(s_silence, 0, sizeof(s_silence));
}

metronome_info_t metronome_audio_info(void)
{
    return s_pub;
}

void metronome_audio_set(int playing, int bpm)
{
    if (bpm >= METRO_BPM_MIN && bpm <= METRO_BPM_MAX) s_bpm = bpm;
    s_running = playing ? 1 : 0;
}

static void audio_task(void *arg)
{
    (void)arg;
    // 等调音任务先把 codec 初始化完(bsp_audio_init 幂等,这里只是错峰)。
    vTaskDelay(pdMS_TO_TICKS(300));
    if (bsp_audio_init() != ESP_OK ||
        bsp_audio_set_format(METRO_FS, 16, 1) != ESP_OK) {
        ESP_LOGE(TAG, "audio init failed, metronome disabled");
        vTaskDelete(NULL);
    }
    bsp_audio_set_volume(60);
    click_buffers_init();
    ESP_LOGI(TAG, "ready (4/4, %d-%d BPM)", METRO_BPM_MIN, METRO_BPM_MAX);

    for (;;) {
        if (!s_running) {
            s_pub.playing = 0;
            vTaskDelay(pdMS_TO_TICKS(60));
            continue;
        }
        s_pub.playing = 1;
        int interval = metronome_interval_samples(s_bpm, METRO_FS);
        s_pub.bpm = s_bpm;
        for (int beat = 0; beat < METRO_BEATS && s_running; beat++) {
            s_pub.beat = beat;
            s_pub.seq++;
            bsp_audio_write(beat == 0 ? s_accent : s_tick,
                            (size_t)CLICK_SAMPLES * sizeof(int16_t));
            int written = CLICK_SAMPLES;
            while (s_running && written < interval) {
                int n = (interval - written) < 512 ? (interval - written) : 512;
                bsp_audio_write(s_silence, (size_t)n * sizeof(int16_t));
                written += n;
            }
        }
    }
}

void metronome_audio_start(void)
{
    if (!s_task) {
        xTaskCreate(audio_task, "metronome", 4096, NULL, 4, &s_task);
    }
}
