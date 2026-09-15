// main/chords_ui.h —— 吉他工具箱界面: 工具箱首页 + 和弦三页 + 调音器 + 变调夹速查。
// 数据在 chord_model.c; 指法图与按键语义见 chords_ui.c 文件头注释。
#pragma once

#include "bsp_button.h"

// 构建首屏(选调页)并载入。需在持有 bsp_lvgl_lock() 的环境下调用。
void chords_ui_enter(void);

// 处理一次按键事件(BSP 按键组件回调转发)。需在持有 bsp_lvgl_lock() 的环境下调用。
void chords_ui_key(bsp_btn_t btn, bsp_btn_ev_t ev);
