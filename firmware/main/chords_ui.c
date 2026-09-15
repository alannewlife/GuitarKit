// main/chords_ui.c —— 吉他和弦词典界面: 选调(KEY) -> 级数列表(LIST, I..vii°) -> 和弦详情(DETAIL)。
// 调音器(TUNER)从 KEY 页长按 OK 进入,再长按或双击 OK 返回。
// 按键语义:
//   上/下 短按   KEY 页=切换调; LIST/DETAIL 页=上一个/下一个级数(循环); TUNER 页=选目标弦
//   确定  短按   KEY/LIST=进入; DETAIL=同一根音上循环拓展变体(三和弦->七和弦->属七...); TUNER=无
//   确定  双击   LIST/DETAIL/TUNER=返回上一级
//   确定  长按   KEY=进调音器; TUNER=返回选调页
// 和弦数据与指法在 chord_model.c; 新增和弦(如 sus4/add9)只需在对应变体表末尾追加。
// 调音器数据来自 tuner_audio 采音任务,本文件只做展示(10Hz 轮询)。
#include "chords_ui.h"
#include "chord_model.h"
#include "tuner.h"
#include "tuner_audio.h"
#include "bsp_button.h"
#include "ui_pixel.h"
#include "lvgl.h"

#include <stdio.h>

typedef enum { PAGE_KEY, PAGE_LIST, PAGE_DETAIL, PAGE_TUNER } page_t;

static page_t s_page = PAGE_KEY;
static int s_key_sel;            // KEY 页当前选中的调
static int s_key;                // 已进入的调
static int s_degree;             // 当前级数
// 会话内记住每级选中的变体, 回到列表时能看到已拓展的和弦名。
static int s_variant[CHORD_KEY_COUNT][CHORD_DEGREE_COUNT];

// 调音页状态: 目标弦 + 自动跟弦(弹哪根亮哪根)。
static int s_tuner_string;
static int s_tuner_seq = -1;     // 上次处理的帧序号
static lv_obj_t *s_tun_freq;     // 大字检测频率
static lv_obj_t *s_tun_status;   // IN TUNE / TUNE UP / TUNE DOWN
static lv_obj_t *s_tun_needle;   // 音分指针
static lv_obj_t *s_tun_target;   // 目标弦频率行
static lv_obj_t *s_tun_pills[TUNER_STRING_COUNT];

static lv_obj_t *s_scr;
static lv_timer_t *s_tick;

static const char *HINT_KEY    = "UP/DN sel  OK open  hold:tuner";
static const char *HINT_LIST   = "UP/DN move  OK open  2xOK back";
static const char *HINT_DETAIL = "UP/DN chord  OK 7th  2xOK back";
static const char *HINT_TUNER  = "UP/DN string  2xOK back";

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
    block(scr, 10, 266, 220, 18, UI_INK);
    lv_obj_t *lab = label_at(scr, text, &lv_font_montserrat_14, 0xFFE9A8, 0, 0);
    lv_obj_set_width(lab, 220);
    lv_obj_set_style_text_align(lab, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_y(lab, 268);
}

// ---- 调音页 ----
// 弦名药丸 + 大字检测频率 + 音分管(指针 ±50 音分) + 松紧提示。
// 数据由 tuner_tick 每 100ms 从采音任务取一次。

static void tuner_pill_refresh(void)
{
    for (int i = 0; i < TUNER_STRING_COUNT; i++) {
        set_selected(s_tun_pills[i], i == s_tuner_string);
    }
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
    tuner_pill_refresh();

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

    s_tun_status = label_at(scr, "listening...", &lv_font_montserrat_20,
                            0xCFE6FF, 0, 216);
    lv_obj_set_width(s_tun_status, 240);
    lv_obj_set_style_text_align(s_tun_status, LV_TEXT_ALIGN_CENTER, 0);

    hint_bar(scr, HINT_TUNER);
}

// 100ms 定时: 取最新检测,刷新频率/指针/状态。指针 -50..+50 音分线性映射。
static void tuner_tick(lv_timer_t *t)
{
    (void)t;
    if (s_page != PAGE_TUNER || !s_scr || !s_tun_freq) return;

    tuner_result_t r = tuner_audio_result();
    if (r.seq == s_tuner_seq) return;                   // 没有新帧
    s_tuner_seq = r.seq;

    if (r.freq_x100 <= 0 || r.nearest < 0) {
        lv_label_set_text(s_tun_freq, "-- Hz");
        lv_obj_set_x(s_tun_needle, 117);
        lv_label_set_text(s_tun_status, "play a string...");
        lv_obj_set_style_text_color(s_tun_status,
                                    lv_color_hex(0xCFE6FF), 0);
        return;
    }

    // 自动跟弦: 弹哪根亮哪根(六弦通用调音器的常见行为)。
    if (r.nearest != s_tuner_string) {
        s_tuner_string = r.nearest;
        tuner_pill_refresh();
    }

    lv_label_set_text_fmt(s_tun_freq, "%d.%02d Hz",
                          r.freq_x100 / 100, r.freq_x100 % 100);

    int c = r.cents;
    if (c < -50) c = -50;
    if (c > 50) c = 50;
    lv_obj_set_x(s_tun_needle, 120 + c * 2 - 3);

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
    const int x0 = 20, y0 = 110;   // 螺母(上弦枕)左端
    const int sp = 16, fh = 26;    // 弦间距 / 品距
    const int width = (CHORD_STRINGS - 1) * sp;

    for (int f = 0; f <= 4; f++) {  // 品线, 第 0 条是加厚的螺母
        int w = (f == 0) ? 4 : 2;
        block(scr, x0, y0 + f * fh - w / 2, width + 2, w, UI_PAPER);
    }
    for (int i = 0; i < CHORD_STRINGS; i++) {
        block(scr, x0 + i * sp - 1, y0, 2, 4 * fh, UI_PAPER);
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
            lv_obj_t *dot = block(scr, x - 8, y0 + row * fh + fh / 2 - 8,
                                  16, 16, UI_YELLOW);
            lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
            lv_obj_set_style_border_width(dot, 2, 0);
            lv_obj_set_style_border_color(dot, lv_color_hex(UI_INK), 0);
            if (ch->finger[i] > 0) {
                lv_obj_t *num = lv_label_create(dot);
                lv_label_set_text_fmt(num, "%d", ch->finger[i]);
                lv_obj_set_style_text_font(num, &lv_font_montserrat_14, 0);
                lv_obj_set_style_text_color(num, lv_color_hex(UI_INK), 0);
                lv_obj_center(num);
            }
        }
    }
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
    s_tuner_seq = -1;

    const char *title;
    if (s_page == PAGE_KEY)         title = "Chord Book";
    else if (s_page == PAGE_LIST)   title = CHORD_KEYS[s_key].name;
    else if (s_page == PAGE_TUNER)  title = "Tuner";
    else {
        const chord_degree_t *deg = &CHORD_KEYS[s_key].degrees[s_degree];
        title = deg->variants[s_variant[s_key][s_degree]].name;
    }

    s_scr = ui_pixel_screen_create(title);
    if (s_page == PAGE_KEY)         build_key_page(s_scr);
    else if (s_page == PAGE_LIST)   build_list_page(s_scr);
    else if (s_page == PAGE_TUNER)  build_tuner_page(s_scr);
    else                            build_detail_page(s_scr);
    lv_screen_load(s_scr);
}

// ---- 对外接口 ----

void chords_ui_enter(void)
{
    s_page = PAGE_KEY;
    s_key_sel = 0;
    s_tuner_string = 0;
    render();
    // 调音数据轮询(10Hz)。常驻整个应用生命周期,回调里自行判断当前页面。
    if (!s_tick) s_tick = lv_timer_create(tuner_tick, 100, NULL);
}

void chords_ui_key(bsp_btn_t btn, bsp_btn_ev_t ev)
{
    if (btn == BSP_BTN_UP || btn == BSP_BTN_DOWN) {
        if (ev != BSP_BTN_CLICK) return;
        int dir = (btn == BSP_BTN_DOWN) ? 1 : -1;
        if (s_page == PAGE_KEY) {
            s_key_sel = (s_key_sel + dir + CHORD_KEY_COUNT) % CHORD_KEY_COUNT;
        } else if (s_page == PAGE_TUNER) {
            s_tuner_string =
                (s_tuner_string + dir + TUNER_STRING_COUNT) % TUNER_STRING_COUNT;
            tuner_pill_refresh();            // 只刷高亮,不整屏重建
            return;
        } else {
            s_degree = (s_degree + dir + CHORD_DEGREE_COUNT) % CHORD_DEGREE_COUNT;
        }
        render();
        return;
    }
    if (btn != BSP_BTN_OK) return;

    if (ev == BSP_BTN_LONG) {                // 长按: 选调页 <-> 调音器
        if (s_page == PAGE_KEY)      s_page = PAGE_TUNER;
        else if (s_page == PAGE_TUNER) s_page = PAGE_KEY;
        render();
        return;
    }
    if (ev == BSP_BTN_DOUBLE) {              // 返回上一级
        if (s_page == PAGE_LIST)      s_page = PAGE_KEY;
        else if (s_page == PAGE_DETAIL) s_page = PAGE_LIST;
        else if (s_page == PAGE_TUNER)  s_page = PAGE_KEY;
        render();
        return;
    }
    if (ev != BSP_BTN_CLICK) return;

    if (s_page == PAGE_KEY) {                // 进入选中的调
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
