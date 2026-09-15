// main/main.c —— 吉他和弦词典(独立应用): 初始化显示与按键, 直接进入和弦界面。
// 没有演示菜单; 不初始化音频/电量(和弦词典用不到, 也少一条失败路径)。
// 按键语义(见 chords_ui.c):
//   上/下 短按   选调页=切换调; 列表/详情页=上一个/下一个级数(循环)
//   确定  短按   选调/列表页=进入; 详情页=同一根音上循环拓展变体
//   确定  双击   返回上一级
#include "bsp_display.h"
#include "bsp_button.h"
#include "bsp_pins.h"      // 错误日志里要打印 BSP_LCD_* 引脚号
#include "chords_ui.h"
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
    ESP_LOGI(TAG, "Guitar Chord Book 启动");

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

    ESP_LOGI(TAG, "和弦词典就绪");
}
