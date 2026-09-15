// main/main.c —— 吉他工具箱(独立应用): 初始化显示/按键/麦克风, 开机进入工具箱。
// 按键语义(见 chords_ui.c 文件头): 双击 OK 逐层返回, 长按 OK 一步回工具箱。
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_pins.h"      // 错误日志里要打印 BSP_LCD_* 引脚号
#include "chords_ui.h"
#include "tuner_audio.h"   // 调音采音任务
#include "lvgl.h"
#include "esp_log.h"

static const char *TAG = "main";

// 按键回调运行在 button 组件的任务里,操作 LVGL 必须加锁。
static void on_key(bsp_btn_t btn, bsp_btn_ev_t ev, void *user) {
    (void)user;
    if (!bsp_lvgl_lock(500)) return;
    chords_ui_key(btn, ev);
    bsp_lvgl_unlock();
}

void app_main(void) {
    ESP_LOGI(TAG, "Guitar Kit 启动");

    // 屏幕是本应用的 UI 载体,失败就没有界面可言 —— 打清楚日志后退出。
    if (bsp_display_init() != ESP_OK || !bsp_lvgl_init()) {
        ESP_LOGE(TAG, "显示/LVGL 初始化失败。"
                      "检查 SPI 接线(MOSI=%d SCLK=%d CS=%d DC=%d BL=%d)",
                 BSP_LCD_MOSI, BSP_LCD_SCLK, BSP_LCD_CS, BSP_LCD_DC, BSP_LCD_BL);
        return;
    }
    bsp_display_backlight(100);

    if (bsp_button_init(on_key, NULL) != ESP_OK) {
        ESP_LOGE(TAG, "按键初始化失败,只能看不能翻页");
    }

    if (bsp_lvgl_lock(1000)) {
        chords_ui_enter();
        bsp_lvgl_unlock();
    }

    // 调音采音任务: 初始化 ES8311(内部含共享 I2C)并持续测频, 供调音页轮询。
    tuner_audio_start();

    ESP_LOGI(TAG, "Guitar Kit 就绪");
}
