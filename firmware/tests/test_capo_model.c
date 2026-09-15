// tests/test_capo_model.c —— 变调夹换算的主机测试。
#include <assert.h>
#include <string.h>
#include "capo_model.h"

int main(void)
{
    /* ---- 指法调表: C G D A E ---- */
    assert(CAPO_SHAPE_KEY_COUNT == 5);
    assert(strcmp(CAPO_SHAPE_KEYS[0].name, "C") == 0);
    assert(CAPO_SHAPE_KEYS[0].pitch_class == 0);
    assert(CAPO_SHAPE_KEYS[1].pitch_class == 7);   // G
    assert(CAPO_SHAPE_KEYS[4].pitch_class == 4);   // E

    /* ---- 调名 ---- */
    assert(strcmp(capo_key_name(0), "C") == 0);
    assert(strcmp(capo_key_name(8), "G#") == 0);
    assert(strcmp(capo_key_name(11), "B") == 0);
    assert(capo_key_name(-1) == NULL);
    assert(capo_key_name(12) == NULL);

    /* ---- 换算 ---- */
    assert(capo_sounding_pc(0, 0) == 0);    // 不夹 + C = C
    assert(capo_sounding_pc(0, 2) == 2);    // 夹 2 + C 指法 = D
    assert(capo_sounding_pc(7, 1) == 8);    // 夹 1 + G 指法 = G#
    assert(capo_sounding_pc(9, 3) == 0);    // 夹 3 + A 指法 = C(跨回绕)
    assert(capo_sounding_pc(4, 7) == 11);   // 夹 7 + E 指法 = B
    assert(capo_sounding_pc(4, CAPO_MAX_FRET) == 11);
    return 0;
}
