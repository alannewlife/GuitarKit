// main/chords_ui.h —— 吉他和弦词典界面: 三个页面(选调/级数列表/和弦详情)。
// 数据在 chord_model.c; 指法图与按键语义见 chords_ui.c 文件头注释。
#pragma once

#include "bsp_button.h"

// 构建首屏(选调页)并载入。需在持有 bsp_lvgl_lock() 的环境下调用。
void chords_ui_enter(void);

// 处理一次按键事件(BSP 按键组件回调转发)。需在持有 bsp_lvgl_lock() 的环境下调用。
void chords_ui_key(bsp_btn_t btn, bsp_btn_ev_t ev);
