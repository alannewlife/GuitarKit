// main/chords_ui.c —— 吉他工具箱界面: 工具箱(HUB) -> 和弦(选调/级数列表/详情) + 调音器 + 变调夹速查。
// 按键语义:
//   上/下 短按   HUB=选工具; KEY=切换调; LIST/DETAIL=上一个/下一个级数(循环); TUNER=选目标弦; CAPO=改数值
//   确定  短按   HUB/KEY/LIST=进入; DETAIL=同一根音上循环拓展变体; TUNER=无; CAPO=切换"夹几品/按什么调"
//   确定  双击   逐层返回(KEY/TUNER/CAPO->HUB, DETAIL->LIST->KEY)
//   确定  三击   任何页面回工具箱
//   确定  长按   任何页面立即息屏(息屏后任意键只唤醒不触发; 播放中的节拍器继续响)
// 和弦数据在 chord_model.c; 调音算法在 tuner.c; 变调夹换算在 capo_model.c; 采音任务在 tuner_audio.c。
// 新工具入口: 在 HUB_ITEMS 表加一行, 网格首页自动多一张卡片。
#include "chords_ui.h"
#include "chord_model.h"
#include "capo_model.h"
#include "metronome.h"
#include "metronome_audio.h"
#include "tuner.h"
#include "tuner_audio.h"
#include "bsp_button.h"
#include "bsp_display.h"      // bsp_display_backlight(息屏/唤醒)
#include "ui_pixel.h"
#include "lvgl.h"

#include <stdio.h>

typedef enum {
    PAGE_HUB, PAGE_KEY, PAGE_LIST, PAGE_DETAIL, PAGE_TUNER, PAGE_CAPO,
    PAGE_METRONOME
} page_t;

// 工具箱入口表: 新工具 = 加一行(名字/副标题/进入哪个页面)。网格两列,
// 单页最多 6 张卡,超出后再考虑翻页。
typedef struct {
    const char *name;
    const char *sub;
    page_t page;
} hub_item_t;

static const hub_item_t HUB_ITEMS[] = {
    { "Chords", "40 chords",  PAGE_KEY   },
    { "Tuner",  "mic pitch",  PAGE_TUNER },
    { "Capo",   "key lookup", PAGE_CAPO  },
    { "Metronome", "4/4 BPM", PAGE_METRONOME },
};
#define HUB_ITEM_COUNT (sizeof(HUB_ITEMS) / sizeof(HUB_ITEMS[0]))

static page_t s_page = PAGE_HUB;
static int s_hub_sel;            // HUB 页当前选中的工具
static int s_key_sel;            // KEY 页当前选中的调
static int s_key;                // 已进入的调
static int s_degree;             // 当前级数
// 会话内记住每级选中的变体, 回到列表时能看到已拓展的和弦名。
static int s_variant[CHORD_KEY_COUNT][CHORD_DEGREE_COUNT];

// 调音页状态: 选定的弦即锁定目标, 偏差始终相对它计算, 不自动跳弦。
static int s_tuner_string;
static int s_tuner_seq = -1;     // 上次处理的帧序号
static lv_obj_t *s_tun_freq;     // 大字检测频率
static lv_obj_t *s_tun_status;   // IN TUNE / TUNE UP / TUNE DOWN
static lv_obj_t *s_tun_needle;   // 音分指针
static lv_obj_t *s_tun_arrow_l;  // 偏差超下限时的左箭头(太低,拧紧)
static lv_obj_t *s_tun_arrow_r;  // 偏差超上限时的右箭头(太高,放松)
static lv_obj_t *s_tun_target;   // 目标弦频率行
static lv_obj_t *s_tun_pills[TUNER_STRING_COUNT];

// 变调夹速查状态: 夹几品(0=不夹) + 用哪个调的指法。
static int s_capo_sel;           // 0=夹几品, 1=指法调
static int s_capo_fret;          // 0..CAPO_MAX_FRET
static int s_capo_shape;         // CAPO_SHAPE_KEYS 下标

// 节拍器状态: 速度/播放开关由 UI 持有, 拍点动画数据来自播放任务。
static int s_met_bpm = 100;
static int s_met_playing;
static int s_met_seq = -1;       // 上次动画处理到的拍序号
static lv_obj_t *s_met_bpm_lab;  // 大字 BPM
static lv_obj_t *s_met_status;   // playing / stopped
static lv_obj_t *s_met_dots[METRO_BEATS];

static lv_obj_t *s_scr;
static lv_timer_t *s_tick;

// 自动息屏: 无操作 2 分钟关背光; 调音页与节拍器播放中视为在用不熄屏;
// 息屏后任意按键只唤醒(不触发动作), 防止半睡状态下误切换。
#define UI_IDLE_OFF_MS (2u * 60u * 1000u)
static int s_backlight_on = 1;
static uint32_t s_idle_ms;

static void metronome_dots_refresh(int beat, int playing);

static const char *HINT_HUB    = "UP/DN sel  OK open  hold:off";
static const char *HINT_KEY    = "UP/DN key  OK open  2xOK back";
static const char *HINT_LIST   = "UP/DN move  OK open  2xOK back";
static const char *HINT_DETAIL = "UP/DN chord  OK 7th  2xOK back";
static const char *HINT_TUNER  = "UP/DN string  2xOK back";
static const char *HINT_CAPO   = "UP/DN set  OK switch  2xOK back";
static const char *HINT_METRO  = "UP/DN +-5  OK play  2xOK back";

// ---- 小工具: 像素风矩形/带边框面板 ----

static lv_obj_t *block(lv_obj_t *parent, int x, int y, int w, int h,
                       uint32_t color)
{
    lv_obj_t *obj = lv_obj_create(parent);
    lv_obj_remove_flag(obj, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_pos(obj, x, y);
    lv_obj_set_size(obj, w, h);
    lv_obj_set_style_radius(obj, 0, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_pad_all(obj, 0, 0);
    lv_obj_set_style_bg_color(obj, lv_color_hex(color), 0);
    return obj;
}

static lv_obj_t *plain_panel(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *p = block(parent, x, y, w, h, UI_PAPER);
    lv_obj_set_style_border_width(p, 3, 0);
    lv_obj_set_style_border_color(p, lv_color_hex(UI_INK), 0);
    return p;
}

static void set_selected(lv_obj_t *p, bool selected)
{
    lv_obj_set_style_bg_color(p,
        lv_color_hex(selected ? UI_YELLOW : UI_PAPER), 0);
    lv_obj_set_style_border_color(p,
        lv_color_hex(selected ? 0xFFFFFF : UI_INK), 0);
}

static lv_obj_t *label_at(lv_obj_t *parent, const char *text,
                          const lv_font_t *font, uint32_t color,
                          int x, int y)
{
    lv_obj_t *lab = lv_label_create(parent);
    lv_label_set_text(lab, text);
    lv_obj_set_style_text_font(lab, font, 0);
    lv_obj_set_style_text_color(lab, lv_color_hex(color), 0);
    lv_obj_set_pos(lab, x, y);
    return lab;
}

static void hint_bar(lv_obj_t *scr, const char *text)
{
    // 提示文字用 12px 字体(14px 在真机上放不下会换行)。单行居中;
    // 万一将来某条超宽, 循环滚动兜底, 绝不换行挤出黑条。
    block(scr, 2, 266, 236, 18, UI_INK);
    lv_obj_t *lab = label_at(scr, text, &lv_font_montserrat_12, 0xFFE9A8, 0, 268);
    lv_obj_set_width(lab, 236);
    lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(lab, LV_LABEL_LONG_SCROLL_CIRCULAR);
}

// ---- 调音页 ----
// 弦名药丸 + 大字检测频率 + 音分管(指针 ±50 音分) + 松紧提示。
// 数据由 tuner_tick 每 100ms 从采音任务取一次。

static void tuner_pill_refresh(void)
{
    // 控件指针可能尚未建全(整屏重建的中途),为空时只跳过,绝不能碰 NULL。
    for (int i = 0; i < TUNER_STRING_COUNT; i++) {
        if (s_tun_pills[i]) set_selected(s_tun_pills[i], i == s_tuner_string);
    }
    if (!s_tun_target) return;
    const tuner_string_t *st = &TUNER_STRINGS[s_tuner_string];
    lv_label_set_text_fmt(s_tun_target, "%s target %d.%02d Hz",
                          st->full, st->freq_x100 / 100, st->freq_x100 % 100);
}

static void build_tuner_page(lv_obj_t *scr)
{
    // 弦名药丸一行: 6 个 34px + 5 个 2px 间隔 = 214, 居中。
    const int pw = 34, ph = 26, gap = 2;
    int x0 = (240 - (TUNER_STRING_COUNT * pw + (TUNER_STRING_COUNT - 1) * gap)) / 2;
    for (int i = 0; i < TUNER_STRING_COUNT; i++) {
        int x = x0 + i * (pw + gap);
        block(scr, x + 3, 52 + 4, pw, ph, UI_INK);      // 阴影
        s_tun_pills[i] = plain_panel(scr, x, 52, pw, ph);
        lv_obj_t *lab = label_at(s_tun_pills[i], TUNER_STRINGS[i].name,
                                 &lv_font_montserrat_14, UI_INK, 0, 3);
        lv_obj_set_width(lab, pw - 6);
        lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
    }

    s_tun_freq = label_at(scr, "-- Hz", &lv_font_montserrat_32,
                          0xFFFFFF, 0, 92);
    lv_obj_set_width(s_tun_freq, 240);
    lv_obj_set_style_text_align(s_tun_freq, LV_TEXT_ALIGN_CENTER, 0);

    s_tun_target = label_at(scr, "", &lv_font_montserrat_14,
                            0xCFE6FF, 0, 132);
    lv_obj_set_width(s_tun_target, 240);
    lv_obj_set_style_text_align(s_tun_target, LV_TEXT_ALIGN_CENTER, 0);

    // 音分管: 220px 对应 ±50 音分(2px/音分),中央 ±5 音分为绿色合弦区。
    block(scr, 10, 168, 220, 10, UI_PAPER);
    block(scr, 110, 168, 20, 10, UI_GRASS);             // ±5 音分
    block(scr, 118, 164, 4, 18, UI_INK);                // 中心标记
    block(scr, 68, 170, 2, 6, UI_INK);                  // -25
    block(scr, 170, 170, 2, 6, UI_INK);                 // +25
    s_tun_needle = block(scr, 117, 156, 6, 34, UI_YELLOW);
    lv_obj_set_style_border_width(s_tun_needle, 2, 0);
    lv_obj_set_style_border_color(s_tun_needle, lv_color_hex(UI_INK), 0);

    // 越界方向箭头: 偏差超出 ±50 音分时出现在对应边, 直观示出"太靠哪边"。
    s_tun_arrow_l = label_at(scr, "<<", &lv_font_montserrat_20,
                             UI_ORANGE, 2, 166);
    s_tun_arrow_r = label_at(scr, ">>", &lv_font_montserrat_20,
                             UI_ORANGE, 216, 166);
    lv_obj_add_flag(s_tun_arrow_l, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_tun_arrow_r, LV_OBJ_FLAG_HIDDEN);

    s_tun_status = label_at(scr, "listening...", &lv_font_montserrat_20,
                            0xCFE6FF, 0, 216);
    lv_obj_set_width(s_tun_status, 240);
    lv_obj_set_style_text_align(s_tun_status, LV_TEXT_ALIGN_CENTER, 0);

    // 全部控件就绪后才刷新高亮与目标行(之前放在这里之前,对 NULL 标签写文本直接崩溃)。
    tuner_pill_refresh();

    hint_bar(scr, HINT_TUNER);
}

// 100ms 定时: 取最新检测,刷新频率/指针/状态。指针 -50..+50 音分线性映射。
static void tuner_tick(lv_timer_t *t)
{
    (void)t;
    // 息屏计时(100ms 一跳): 调音页/节拍器播放中不计时。
    if (!s_backlight_on) return;
    if (s_page == PAGE_TUNER || s_met_playing) {
        s_idle_ms = 0;
    } else if ((s_idle_ms += 100) >= UI_IDLE_OFF_MS) {
        s_idle_ms = 0;
        s_backlight_on = 0;
        bsp_display_backlight(0);
        return;
    }

    if (s_page == PAGE_METRONOME && s_scr) {
        metronome_info_t m = metronome_audio_info();
        if (m.seq != s_met_seq) {
            s_met_seq = m.seq;
            s_met_playing = m.playing;
            metronome_dots_refresh(m.playing ? m.beat : -1, m.playing);
            if (s_met_status) {
                lv_label_set_text(s_met_status, m.playing ? "playing" : "stopped");
                lv_obj_set_style_text_color(s_met_status,
                    lv_color_hex(m.playing ? 0x8FE84C : 0xCFE6FF), 0);
            }
        }
        return;
    }
    if (s_page != PAGE_TUNER || !s_scr || !s_tun_freq) return;

    tuner_result_t r = tuner_audio_result();
    if (r.seq == s_tuner_seq) return;                   // 没有新帧
    s_tuner_seq = r.seq;

    if (r.freq_x100 <= 0) {
        lv_label_set_text(s_tun_freq, "-- Hz");
        lv_obj_set_x(s_tun_needle, 117);
        lv_label_set_text(s_tun_status, "play a string...");
        lv_obj_set_style_text_color(s_tun_status,
                                    lv_color_hex(0xCFE6FF), 0);
        return;
    }

    // 定弦: 偏差始终相对当前选中的弦, 不随检测到的音高跳到别的弦。
    int c = tuner_cents(TUNER_STRINGS[s_tuner_string].freq_x100, r.freq_x100);

    lv_label_set_text_fmt(s_tun_freq, "%d.%02d Hz",
                          r.freq_x100 / 100, r.freq_x100 % 100);

    int pin = c;
    if (pin < -50) pin = -50;
    if (pin > 50) pin = 50;
    lv_obj_set_x(s_tun_needle, 120 + pin * 2 - 3);

    // 越界箭头: 出界才亮对应边, 入界全灭。
    if (c < -50) lv_obj_remove_flag(s_tun_arrow_l, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(s_tun_arrow_l, LV_OBJ_FLAG_HIDDEN);
    if (c > 50)  lv_obj_remove_flag(s_tun_arrow_r, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(s_tun_arrow_r, LV_OBJ_FLAG_HIDDEN);

    if (c > -6 && c < 6) {
        lv_label_set_text(s_tun_status, "IN TUNE");
        lv_obj_set_style_text_color(s_tun_status,
                                    lv_color_hex(0x8FE84C), 0);
    } else if (c < 0) {
        lv_label_set_text(s_tun_status, "TUNE UP (flat)");
        lv_obj_set_style_text_color(s_tun_status,
                                    lv_color_hex(UI_YELLOW), 0);
    } else {
        lv_label_set_text(s_tun_status, "TUNE DOWN (sharp)");
        lv_obj_set_style_text_color(s_tun_status,
                                    lv_color_hex(UI_ORANGE), 0);
    }
}

// ---- 和弦指法图 ----
// 6 弦 4 品的开放把位图。chord_model 的数据保证最高品 <=4
// (tests/test_chord_model.c 里有不变量检查), 因此不做移动把位绘制。

static void draw_diagram(lv_obj_t *scr, const chord_variant_t *ch)
{
    const int x0 = 20, y0 = 126;   // 螺母(上弦枕)左端
    const int sp = 16, fh = 26;    // 弦间距 / 品距
    const int width = (CHORD_STRINGS - 1) * sp;

    for (int f = 0; f <= 4; f++) {  // 品线, 第 0 条是加厚的螺母
        int w = (f == 0) ? 4 : 2;
        block(scr, x0, y0 + f * fh - w / 2, width + 2, w, UI_PAPER);
    }
    for (int i = 0; i < CHORD_STRINGS; i++) {
        // 根音所在的弦: 橙色加粗, 一眼看出根音位置。
        const int is_root = (i == ch->root_string);
        block(scr, x0 + i * sp - 1, y0, is_root ? 3 : 2, 4 * fh,
              is_root ? UI_ORANGE : UI_PAPER);
    }

    for (int i = 0; i < CHORD_STRINGS; i++) {
        int x = x0 + i * sp;
        if (ch->fret[i] < 0) {                     // 闷音 x
            lv_obj_t *m = label_at(scr, "x", &lv_font_montserrat_14,
                                   UI_PAPER, x - 8, y0 - 20);
            lv_obj_set_width(m, 16);
            lv_obj_set_style_text_align(m, LV_TEXT_ALIGN_CENTER, 0);
        } else if (ch->fret[i] == 0) {             // 空弦 o
            lv_obj_t *m = label_at(scr, "o", &lv_font_montserrat_14,
                                   UI_PAPER, x - 8, y0 - 20);
            lv_obj_set_width(m, 16);
            lv_obj_set_style_text_align(m, LV_TEXT_ALIGN_CENTER, 0);
        } else {                                   // 按法点 + 指法号
            int row = ch->fret[i] - 1;
            const int is_root = (i == ch->root_string);
            lv_obj_t *dot = block(scr, x - 8, y0 + row * fh + fh / 2 - 8,
                                  16, 16, is_root ? UI_RED : UI_YELLOW);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(dot, 2, 0);
            lv_obj_set_style_border_color(dot, lv_color_hex(UI_INK), 0);
            if (ch->finger[i] > 0) {
                lv_obj_t *num = lv_label_create(dot);
                lv_label_set_text_fmt(num, "%d", ch->finger[i]);
                lv_obj_set_style_text_font(num, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(num,
                    lv_color_hex(is_root ? 0xFFFFFF : UI_INK), 0);
                lv_obj_center(num);
            }
        }
    }
}

// ---- 工具箱首页 ----
// 两列卡片网格(104x56, 行距 68, 单页 3 行 6 卡)。卡片来自 HUB_ITEMS 表。

static void build_hub_page(lv_obj_t *scr)
{
    for (size_t i = 0; i < HUB_ITEM_COUNT; i++) {
        const int col = (int)(i % 2), row = (int)(i / 2);
        const int x = 10 + col * 116, y = 56 + row * 68;
        block(scr, x + 3, y + 4, 104, 56, UI_INK);      // 阴影
        lv_obj_t *card = plain_panel(scr, x, y, 104, 56);
        set_selected(card, (int)i == s_hub_sel);

        lv_obj_t *name = label_at(card, HUB_ITEMS[i].name,
                                  &lv_font_montserrat_14, UI_INK, 0, 8);
        lv_obj_set_width(name, 104 - 6);
        lv_obj_set_style_text_align(name, LV_TEXT_ALIGN_CENTER, 0);

        lv_obj_t *sub = label_at(card, HUB_ITEMS[i].sub,
                                 &lv_font_montserrat_14, 0x5A6B7A, 0, 0);
        lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -8);
    }
    hint_bar(scr, HINT_HUB);
}

// ---- 变调夹速查 ----
// 两行选择(夹几品 / 指法调) + 大字结果。UP/DN 改选中行的值, OK 切换行。

static void build_capo_page(lv_obj_t *scr)
{
    char shape_val[8], fret_val[8], result[40];
    if (s_capo_fret == 0) snprintf(fret_val, sizeof(fret_val), "none");
    else snprintf(fret_val, sizeof(fret_val), "%d", s_capo_fret);
    snprintf(shape_val, sizeof(shape_val), "%s",
             CAPO_SHAPE_KEYS[s_capo_shape].name);

    const char *labels[2] = { "Capo fret", "Shape key" };
    const char *values[2] = { fret_val, shape_val };
    for (int i = 0; i < 2; i++) {
        block(scr, 29, 60 + i * 52, 188, 44, UI_INK);   // 阴影
        lv_obj_t *row = plain_panel(scr, 26, 56 + i * 52, 188, 44);
        set_selected(row, s_capo_sel == i);
        label_at(row, labels[i], &lv_font_montserrat_14, UI_SKY_DARK, 10, 13);
        lv_obj_t *val = label_at(row, values[i], &lv_font_montserrat_20,
                                 UI_INK, 0, 8);
        lv_obj_align(val, LV_ALIGN_RIGHT_MID, -12, 0);
    }

    // 大字结果: 实际的调。
    int pc = capo_sounding_pc(CAPO_SHAPE_KEYS[s_capo_shape].pitch_class,
                              s_capo_fret);
    snprintf(result, sizeof(result), "= %s", capo_key_name(pc));
    lv_obj_t *big = label_at(scr, result, &lv_font_montserrat_32,
                             0xFFFFFF, 0, 182);
    lv_obj_set_width(big, 240);
    lv_obj_set_style_text_align(big, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *cap = label_at(scr, "sounding key", &lv_font_montserrat_14,
                             0xCFE6FF, 0, 226);
    lv_obj_set_width(cap, 240);
    lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, 0);

    hint_bar(scr, HINT_CAPO);
}

// ---- 页面构建 ----

static void build_key_page(lv_obj_t *scr)
{
    for (int i = 0; i < CHORD_KEY_COUNT; i++) {
        // 196x88 大卡片: 14px 副标题 "7 degrees + 7th chords" 需要约 165px,
        // 卡片内宽 196-8(边框)-14(内边距) = 174px 才放得下。
        lv_obj_t *card = ui_pixel_panel_create(scr, 22, 64 + i * 100,
                                               196, 88, UI_PAPER);
        ui_pixel_set_selected(card, i == s_key_sel, true);
        lv_obj_t *big = lv_label_create(card);
        lv_label_set_text(big, CHORD_KEYS[i].name);
        lv_obj_set_style_text_font(big, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(big, lv_color_hex(UI_INK), 0);
        lv_obj_align(big, LV_ALIGN_TOP_MID, 0, 16);

        lv_obj_t *sub = lv_label_create(card);
        lv_label_set_text(sub, "7 degrees + 7th chords");
        lv_obj_set_style_text_font(sub, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(sub, lv_color_hex(0x5A6B7A), 0);
        lv_obj_align(sub, LV_ALIGN_BOTTOM_MID, 0, -12);
    }
    hint_bar(scr, HINT_KEY);
}

static void build_list_page(lv_obj_t *scr)
{
    const chord_degree_t *degrees = CHORD_KEYS[s_key].degrees;
    for (int i = 0; i < CHORD_DEGREE_COUNT; i++) {
        int vi = s_variant[s_key][i];
        const chord_variant_t *ch = &degrees[i].variants[vi];

        block(scr, 29, 56 + i * 30, 188, 26, UI_INK);   // 阴影
        lv_obj_t *row = plain_panel(scr, 26, 52 + i * 30, 188, 26);
        set_selected(row, i == s_degree);

        label_at(row, degrees[i].roman, &lv_font_montserrat_14,
                 UI_SKY_DARK, 8, 3);
        label_at(row, ch->name, &lv_font_montserrat_14, UI_INK, 56, 3);
        lv_obj_t *q = label_at(row, ch->quality, &lv_font_montserrat_14,
                               0x6B7B8A, 0, 3);
        lv_obj_align(q, LV_ALIGN_RIGHT_MID, -8, 0);
    }
    hint_bar(scr, HINT_LIST);
}

static void build_detail_page(lv_obj_t *scr)
{
    const chord_degree_t *deg = &CHORD_KEYS[s_key].degrees[s_degree];
    int vi = s_variant[s_key][s_degree];
    const chord_variant_t *ch = &deg->variants[vi];

    lv_obj_t *badge = block(scr, 12, 48, 56, 22, UI_INK);
    lv_obj_t *roman = label_at(badge, deg->roman, &lv_font_montserrat_14,
                               UI_YELLOW, 0, 0);
    lv_obj_set_width(roman, 56);
    lv_obj_set_style_text_align(roman, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_y(roman, 3);

    label_at(scr, deg->func, &lv_font_montserrat_14, 0xEAF4FF, 12, 76);

    // 大和弦名占位在标题牌右侧天空上, 与上/下列互不重叠。
    lv_obj_t *name = label_at(scr, ch->name, &lv_font_montserrat_32,
                              0xFFFFFF, 124, 44);
    lv_obj_set_style_text_color(name, lv_color_hex(0xFFFFFF), 0);

    draw_diagram(scr, ch);

    label_at(scr, ch->quality, &lv_font_montserrat_20, UI_YELLOW, 128, 88);
    label_at(scr, ch->notes, &lv_font_montserrat_14, 0xEAF4FF, 128, 118);
    if (ch->hint[0] != '\0') {
        label_at(scr, ch->hint, &lv_font_montserrat_14, UI_ORANGE, 128, 142);
    }

    // 拓展变体指示点: 当前变体亮黄。
    for (int i = 0; i < deg->variant_count; i++) {
        block(scr, 128 + i * 15, 236, 9, 9,
              i == vi ? UI_YELLOW : 0x9DC9E8);
    }
    char pos[32];
    snprintf(pos, sizeof(pos), "%d/%d", vi + 1, deg->variant_count);
    label_at(scr, pos, &lv_font_montserrat_14, 0xFFFFFF,
             128 + deg->variant_count * 15 + 6, 232);

    hint_bar(scr, HINT_DETAIL);
}

// ---- 节拍器 ----
// 大字 BPM + 四个拍点(小节头重音)。播放由 metronome_audio 任务发声,
// 本页 100ms tick 读任务快照刷新拍点与状态; UP/DN 改速度即时生效。

static void metronome_dots_refresh(int beat, int playing)
{
    for (int i = 0; i < METRO_BEATS; i++) {
        if (!s_met_dots[i]) continue;
        lv_obj_set_style_bg_color(s_met_dots[i],
            lv_color_hex(i == beat ? UI_YELLOW : UI_PAPER), 0);
        lv_obj_set_style_border_width(s_met_dots[i],
            (i == beat && playing) ? 3 : 2, 0);
    }
}

static void build_metronome_page(lv_obj_t *scr)
{
    char bpm_text[16];
    snprintf(bpm_text, sizeof(bpm_text), "%d", s_met_bpm);
    s_met_bpm_lab = label_at(scr, bpm_text, &lv_font_montserrat_32,
                             0xFFFFFF, 0, 74);
    lv_obj_set_width(s_met_bpm_lab, 240);
    lv_obj_set_style_text_align(s_met_bpm_lab, LV_TEXT_ALIGN_CENTER, 0);

    lv_obj_t *cap = label_at(scr, "BPM  4/4", &lv_font_montserrat_14,
                             0xCFE6FF, 0, 112);
    lv_obj_set_width(cap, 240);
    lv_obj_set_style_text_align(cap, LV_TEXT_ALIGN_CENTER, 0);

    // 四个拍点: 22px 圆, 间距 18, 居中。
    const int d = 22, gap = 18;
    int x0 = (240 - (METRO_BEATS * d + (METRO_BEATS - 1) * gap)) / 2;
    for (int i = 0; i < METRO_BEATS; i++) {
        s_met_dots[i] = block(scr, x0 + i * (d + gap), 152, d, d,
                              i == 0 ? UI_YELLOW : UI_PAPER);
        lv_obj_set_style_radius(s_met_dots[i], LV_RADIUS_CIRCLE, 0);
        lv_obj_set_style_border_width(s_met_dots[i], 2, 0);
        lv_obj_set_style_border_color(s_met_dots[i], lv_color_hex(UI_INK), 0);
    }

    s_met_status = label_at(scr, s_met_playing ? "playing" : "stopped",
                            &lv_font_montserrat_20, 0xCFE6FF, 0, 206);
    lv_obj_set_width(s_met_status, 240);
    lv_obj_set_style_text_align(s_met_status, LV_TEXT_ALIGN_CENTER, 0);

    hint_bar(scr, HINT_METRO);
}

// 每次状态变化整屏重建。必须先删旧屏再建新屏: LVGL 内存池有限,
// 两屏对象短暂共存就会耗尽池子导致设备重启(真机上表现为"跳回主菜单")。
static void render(void)
{
    if (s_scr) {
        lv_obj_delete(s_scr);
        s_scr = NULL;
    }
    // 旧屏上的调音控件指针随屏销毁,先清空,防止 tick 访问悬垂指针。
    for (int i = 0; i < TUNER_STRING_COUNT; i++) s_tun_pills[i] = NULL;
    s_tun_freq = s_tun_status = s_tun_needle = s_tun_target = NULL;
    s_tun_arrow_l = s_tun_arrow_r = NULL;
    s_tuner_seq = -1;
    for (int i = 0; i < METRO_BEATS; i++) s_met_dots[i] = NULL;
    s_met_bpm_lab = s_met_status = NULL;
    s_met_seq = -1;
    // 离开节拍器页就停止播放(切到别的工具不该继续嗒嗒响)。
    if (s_page != PAGE_METRONOME) { metronome_audio_set(0, s_met_bpm); s_met_playing = 0; }

    const char *title;
    if (s_page == PAGE_HUB)         title = "Guitar Kit";
    else if (s_page == PAGE_KEY)    title = "Chord Book";
    else if (s_page == PAGE_TUNER)  title = "Tuner";
    else if (s_page == PAGE_CAPO)   title = "Capo";
    else if (s_page == PAGE_METRONOME) title = "Metronome";
    else                            title = CHORD_KEYS[s_key].name;   // LIST/DETAIL 均显示调名

    s_scr = ui_pixel_screen_create(title);
    if (s_page == PAGE_HUB)         build_hub_page(s_scr);
    else if (s_page == PAGE_KEY)    build_key_page(s_scr);
    else if (s_page == PAGE_LIST)   build_list_page(s_scr);
    else if (s_page == PAGE_TUNER)  build_tuner_page(s_scr);
    else if (s_page == PAGE_CAPO)   build_capo_page(s_scr);
    else if (s_page == PAGE_METRONOME) build_metronome_page(s_scr);
    else                            build_detail_page(s_scr);
    lv_screen_load(s_scr);
}

// ---- 对外接口 ----

void chords_ui_enter(void)
{
    s_page = PAGE_HUB;
    s_hub_sel = 0;
    s_tuner_string = 0;
    render();
    // 调音数据轮询(10Hz)。常驻整个应用生命周期,回调里自行判断当前页面。
    if (!s_tick) s_tick = lv_timer_create(tuner_tick, 100, NULL);
}

void chords_ui_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    s_idle_ms = 0;                       // 任意按键都算一次活动
    if (!s_backlight_on) {               // 息屏中: 只唤醒,不处理动作
        s_backlight_on = 1;
        bsp_display_backlight(100);
        return;
    }
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        if (ev != BSP_BTN_CLICK) return;
        int dir = (btn == BSP_BTN_DOWN) ? 1 : -1;
        if (s_page == PAGE_HUB) {
            int n = (int)HUB_ITEM_COUNT;
            s_hub_sel = (s_hub_sel + dir + n) % n;
        } else if (s_page == PAGE_KEY) {
            s_key_sel = (s_key_sel + dir + CHORD_KEY_COUNT) % CHORD_KEY_COUNT;
        } else if (s_page == PAGE_TUNER) {
            s_tuner_string =
                (s_tuner_string + dir + TUNER_STRING_COUNT) % TUNER_STRING_COUNT;
            tuner_pill_refresh();            // 只刷高亮,不整屏重建
            return;
        } else if (s_page == PAGE_CAPO) {
            if (s_capo_sel == 0) {
                s_capo_fret = (s_capo_fret + dir + CAPO_MAX_FRET + 1)
                              % (CAPO_MAX_FRET + 1);
            } else {
                s_capo_shape = (s_capo_shape + dir + CAPO_SHAPE_KEY_COUNT)
                               % CAPO_SHAPE_KEY_COUNT;
            }
        } else if (s_page == PAGE_METRONOME) {
            s_met_bpm = metronome_clamp_bpm(s_met_bpm + dir * 5);
            metronome_audio_set(s_met_playing, s_met_bpm);   // 播放中改速度即时生效
        } else {
            s_degree = (s_degree + dir + CHORD_DEGREE_COUNT) % CHORD_DEGREE_COUNT;
        }
        render();
        return;
    }
    if (btn != BSP_BTN_OK) return;

    if (ev == BSP_BTN_TRIPLE) {              // 三击: 任何页面回工具箱
        if (s_page != PAGE_HUB) {
            s_page = PAGE_HUB;
            render();                        // 离开节拍器页时 render 会停掉播放
        }
        return;
    }
    if (ev == BSP_BTN_LONG) {                // 长按: 任何页面立即息屏(播放中的节拍器继续响)
        s_idle_ms = 0;
        s_backlight_on = 0;
        bsp_display_backlight(0);
        return;
    }
    if (ev == BSP_BTN_DOUBLE) {              // 逐层返回
        if (s_page == PAGE_LIST)      s_page = PAGE_KEY;
        else if (s_page == PAGE_DETAIL) s_page = PAGE_LIST;
        else                          s_page = PAGE_HUB; // KEY/TUNER/CAPO/METRONOME 及以后的新工具
        render();
        return;
    }
    if (ev != BSP_BTN_CLICK) return;

    if (s_page == PAGE_HUB) {                // 进入选中的工具
        s_page = HUB_ITEMS[s_hub_sel].page;
        if (s_page == PAGE_KEY) { s_key = s_key_sel; s_degree = 0; }
    } else if (s_page == PAGE_CAPO) {        // 切换"夹几品 / 指法调"
        s_capo_sel = 1 - s_capo_sel;
    } else if (s_page == PAGE_METRONOME) {   // 开始/停止
        s_met_playing = !s_met_playing;
        metronome_audio_set(s_met_playing, s_met_bpm);
    } else if (s_page == PAGE_KEY) {         // 进入选中的调
        s_key = s_key_sel;
        s_degree = 0;
        s_page = PAGE_LIST;
    } else if (s_page == PAGE_LIST) {        // 查看指法
        s_page = PAGE_DETAIL;
    } else if (s_page == PAGE_DETAIL) {      // 拓展: 同根音循环切换变体
        int n = chord_variant_count(s_key, s_degree);
        if (n > 0) {
            s_variant[s_key][s_degree] = (s_variant[s_key][s_degree] + 1) % n;
        }
    }
    render();                                // TUNER 页 OK 单击无动作,render 无害
}
