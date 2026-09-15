// main/tuner_audio.c —— 麦克风采音 + 测频任务。
// bsp_audio_read 是阻塞调用,必须放在独立任务里(不能占 LVGL/按键回调)。
// 每帧 1024 采样(16kHz = 64ms,低音 E2 约 5 个周期)做一次检测,
// 结果写入单写者/单读者的发布结构,UI 定时轮询。
#include "tuner_audio.h"
#include "bsp_audio.h"
#include "tuner.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#define TUNER_FS      16000
#define TUNER_FRAME   1024      // 64ms 一帧
#define TAG "tuner_audio"

static tuner_result_t s_pub;    // 最新结果(seq 最后写,配对错位最多差一帧)
static TaskHandle_t s_task;
static int s_center = -1;       // 锁定弦的目标频率 x100; -1 = 全范围

void tuner_audio_set_string(int string_idx)
{
    s_center = (string_idx >= 0 && string_idx < TUNER_STRING_COUNT)
        ? TUNER_STRINGS[string_idx].freq_x100 : -1;
}

tuner_result_t tuner_audio_result(void)
{
    return s_pub;
}

static void audio_task(void *arg)
{
    (void)arg;
    // 采集缓冲放静态区(2KB),避免占用 6KB 的任务栈(检测函数还有局部 VLA)。
    static int16_t buf[TUNER_FRAME];

    if (bsp_audio_init() != ESP_OK ||
        bsp_audio_set_format(TUNER_FS, 16, 1) != ESP_OK) {
        ESP_LOGE(TAG, "audio init failed, tuner disabled");
        vTaskDelete(NULL);
    }
    ESP_LOGI(TAG, "mic ready @%dHz", TUNER_FS);

    // 三帧中值滤波的滑动缓冲: 单帧毛刺(噪声/误检)不会传到界面。
    int hist[3] = { 0, 0, 0 };

    for (;;) {
        if (bsp_audio_read(buf, sizeof(buf)) != ESP_OK) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        int f = (s_center > 0)
            ? tuner_detect_freq_x100_near(buf, TUNER_FRAME, TUNER_FS, s_center)
            : tuner_detect_freq_x100(buf, TUNER_FRAME, TUNER_FS);

        hist[0] = hist[1];
        hist[1] = hist[2];
        hist[2] = f;
        int a = hist[0], b = hist[1], c = hist[2];
        int lo = a < b ? a : b, hi = a > b ? a : b;
        int med = c < lo ? lo : (c > hi ? hi : c);   // 三数取中

        int nearest = -1, cents = 0;
        if (med > 0) {
            nearest = tuner_nearest_string(med);
            if (nearest >= 0) {
                cents = tuner_cents(TUNER_STRINGS[nearest].freq_x100, med);
            }
        }
        s_pub.freq_x100 = med;
        s_pub.nearest = nearest;
        s_pub.cents = cents;
        s_pub.seq++;                 // 最后写:读者看到 seq 变了则数据已就绪
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

void tuner_audio_start(void)
{
    if (!s_task) {
        xTaskCreate(audio_task, "tuner_audio", 6144, NULL, 4, &s_task);
    }
}
