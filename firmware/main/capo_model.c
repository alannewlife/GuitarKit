// main/capo_model.c —— 变调夹换算: 夹品 = 把指法调整体上移 fret 个半音。
#include "capo_model.h"

#include <stddef.h>

const capo_key_t CAPO_SHAPE_KEYS[CAPO_SHAPE_KEY_COUNT] = {
    { "C", 0 },
    { "G", 7 },
    { "D", 2 },
    { "A", 9 },
    { "E", 4 },
};

static const char *const KEY_NAMES[12] = {
    "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B",
};

const char *capo_key_name(int pitch_class)
{
    if (pitch_class < 0 || pitch_class > 11) return NULL;
    return KEY_NAMES[pitch_class];
}

int capo_sounding_pc(int shape_pc, int fret)
{
    return ((shape_pc + fret) % 12 + 12) % 12;
}
