// tests/test_chord_model.c —— 和弦数据的主机测试(无需硬件)。
// 重点: 逐项比对全部 40 个指法的品位表(防数据笔误), 并检查指法不变量:
// 最高品 <=4(固件指法图只画 4 品)、按品必有指法、闷音/空弦必无指法。
#include <assert.h>
#include <string.h>
#include "chord_model.h"

static int fret_from_char(char c)
{
    if (c == 'x') return -1;
    assert(c >= '0' && c <= '9');
    return c - '0';
}

// frets 形如 "x32010"(弦6 -> 弦1), 与 chord_model.c 的数据逐项对照。
static void expect_variant(int key, int degree, int variant,
                           const char *name, const char *frets)
{
    const chord_variant_t *ch = chord_variant_get(key, degree, variant);
    assert(ch != NULL);
    assert(strcmp(ch->name, name) == 0);
    assert(strlen(frets) == CHORD_STRINGS);

    int sounding = 0;
    for (int i = 0; i < CHORD_STRINGS; i++) {
        int want = fret_from_char(frets[i]);
        assert(ch->fret[i] == want);
        if (want > 0) {
            assert(ch->finger[i] >= 1 && ch->finger[i] <= 4);
            assert(want <= 4);          // 指法图只画 4 品
            sounding++;
        } else {
            assert(ch->finger[i] == 0);
            if (want == 0) sounding++;
        }
    }
    assert(sounding > 0);               // 至少有一根弦发声
}

static void expect_key_shape(int key, const char *key_name)
{
    assert(strcmp(CHORD_KEYS[key].name, key_name) == 0);
    for (int d = 0; d < CHORD_DEGREE_COUNT; d++) {
        const chord_degree_t *deg = &CHORD_KEYS[key].degrees[d];
        assert(deg->variants != NULL);
        // vii° 只有 2 个变体(减三/半减七), 其余级数 3 个。
        assert(chord_variant_count(key, d) == (d == 6 ? 2 : 3));
        assert(deg->variant_count == chord_variant_count(key, d));
    }
}

int main(void)
{
    /* ---- 结构 ---- */
    assert(CHORD_KEY_COUNT == 2);
    expect_key_shape(0, "C Major");
    expect_key_shape(1, "G Major");

    assert(strcmp(CHORD_KEYS[0].degrees[6].roman, "vii°") == 0);
    assert(strcmp(CHORD_KEYS[0].degrees[4].func, "Dominant") == 0);

    /* ---- C 大调全部指法 ---- */
    expect_variant(0, 0, 0, "C",     "x32010");
    expect_variant(0, 0, 1, "Cmaj7", "x32000");
    expect_variant(0, 0, 2, "C7",    "x32310");
    expect_variant(0, 1, 0, "Dm",    "xx0231");
    expect_variant(0, 1, 1, "Dm7",   "xx0211");
    expect_variant(0, 1, 2, "D7",    "xx0212");
    expect_variant(0, 2, 0, "Em",    "022000");
    expect_variant(0, 2, 1, "Em7",   "020000");
    expect_variant(0, 2, 2, "E7",    "020100");
    expect_variant(0, 3, 0, "F",     "xx3211");
    expect_variant(0, 3, 1, "Fmaj7", "xx3210");
    expect_variant(0, 3, 2, "F7",    "131211");
    expect_variant(0, 4, 0, "G",     "320003");
    expect_variant(0, 4, 1, "Gmaj7", "320002");
    expect_variant(0, 4, 2, "G7",    "320001");
    expect_variant(0, 5, 0, "Am",    "x02210");
    expect_variant(0, 5, 1, "Am7",   "x02010");
    expect_variant(0, 5, 2, "A7",    "x02020");
    expect_variant(0, 6, 0, "Bdim",  "x2343x");
    expect_variant(0, 6, 1, "Bm7b5", "x2323x");

    /* ---- G 大调全部指法 ---- */
    expect_variant(1, 0, 0, "G",      "320003");
    expect_variant(1, 0, 1, "Gmaj7",  "320002");
    expect_variant(1, 0, 2, "G7",     "320001");
    expect_variant(1, 1, 0, "Am",     "x02210");
    expect_variant(1, 1, 1, "Am7",    "x02010");
    expect_variant(1, 1, 2, "A7",     "x02020");
    expect_variant(1, 2, 0, "Bm",     "x24432");
    expect_variant(1, 2, 1, "Bm7",    "x2020x");
    expect_variant(1, 2, 2, "B7",     "x21202");
    expect_variant(1, 3, 0, "C",      "x32010");
    expect_variant(1, 3, 1, "Cmaj7",  "x32000");
    expect_variant(1, 3, 2, "C7",     "x32310");
    expect_variant(1, 4, 0, "D",      "xx0232");
    expect_variant(1, 4, 1, "Dmaj7",  "xx0222");
    expect_variant(1, 4, 2, "D7",     "xx0212");
    expect_variant(1, 5, 0, "Em",     "022000");
    expect_variant(1, 5, 1, "Em7",    "020000");
    expect_variant(1, 5, 2, "E7",     "020100");
    expect_variant(1, 6, 0, "F#dim",  "xxx212");
    expect_variant(1, 6, 1, "F#m7b5", "2x2210");

    /* ---- 文案抽查 ---- */
    assert(strcmp(chord_variant_get(0, 4, 1)->notes, "G B D F#") == 0);
    assert(strcmp(chord_variant_get(0, 3, 0)->hint, "Easy F") == 0);
    assert(strcmp(chord_variant_get(1, 6, 0)->hint, "No low root") == 0);
    assert(strcmp(chord_variant_get(1, 4, 1)->quality, "Maj 7") == 0);

    /* ---- 访问器越界保护 ---- */
    assert(chord_variant_get(-1, 0, 0) == NULL);
    assert(chord_variant_get(0, -1, 0) == NULL);
    assert(chord_variant_get(0, 0, -1) == NULL);
    assert(chord_variant_get(0, 0, 3) == NULL);   // 变体越界
    assert(chord_variant_get(0, 6, 2) == NULL);   // vii° 只有 2 个
    assert(chord_variant_get(0, 7, 0) == NULL);
    assert(chord_variant_get(2, 0, 0) == NULL);
    assert(chord_variant_count(-1, 0) == 0);
    assert(chord_variant_count(0, 7) == 0);

    return 0;
}
