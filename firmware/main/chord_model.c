// main/chord_model.c —— C 大调 / G 大调和弦表。
// 指法与 docs/CHANGELOG 记录的交互示意页(guitar-chords/index.html)逐项一致:
// 每级第一个变体是调内三和弦(最常用), 之后是该级调内七和弦与跨性质七和弦。
// vii° 的减三和弦给了简化/转位按法(均在开放把位, 数据保证最高品 <=4)。
#include "chord_model.h"

#include <stddef.h>

// V(...) 的位序: 弦6(低E) -> 弦1(高e)。
#define V(name_, quality_, notes_, hint_,                               \
          f6, f5, f4, f3, f2, f1,                                       \
          g6, g5, g4, g3, g2, g1)                                       \
    { .name = (name_), .quality = (quality_), .notes = (notes_),        \
      .hint = (hint_),                                                  \
      .fret = { (f6), (f5), (f4), (f3), (f2), (f1) },                   \
      .finger = { (g6), (g5), (g4), (g3), (g2), (g1) } }

/* ---------------- C 大调 ---------------- */

static const chord_variant_t C_DEG_I[] = {
    V("C",     "Major", "C E G",     "",       -1, 3, 2, 0, 1, 0,   0, 3, 2, 0, 1, 0),
    V("Cmaj7", "Maj 7", "C E G B",   "",       -1, 3, 2, 0, 0, 0,   0, 3, 2, 0, 0, 0),
    V("C7",    "Dom 7", "C E G Bb",  "",       -1, 3, 2, 3, 1, 0,   0, 3, 2, 4, 1, 0),
};
static const chord_variant_t C_DEG_II[] = {
    V("Dm",    "Minor", "D F A",     "",       -1, -1, 0, 2, 3, 1, 0, 0, 0, 2, 3, 1),
    V("Dm7",   "Min 7", "D F A C",   "",       -1, -1, 0, 2, 1, 1, 0, 0, 0, 2, 1, 1),
    V("D7",    "Dom 7", "D F# A C",  "",       -1, -1, 0, 2, 1, 2, 0, 0, 0, 2, 1, 3),
};
static const chord_variant_t C_DEG_III[] = {
    V("Em",    "Minor", "E G B",     "",        0, 2, 2, 0, 0, 0,   0, 2, 3, 0, 0, 0),
    V("Em7",   "Min 7", "E G B D",   "",        0, 2, 0, 0, 0, 0,   0, 2, 0, 0, 0, 0),
    V("E7",    "Dom 7", "E G# B D",  "",        0, 2, 0, 1, 0, 0,   0, 2, 0, 1, 0, 0),
};
static const chord_variant_t C_DEG_IV[] = {
    V("F",     "Major", "F A C",     "Easy F",  -1, -1, 3, 2, 1, 1, 0, 0, 3, 2, 1, 1),
    V("Fmaj7", "Maj 7", "F A C E",   "",        -1, -1, 3, 2, 1, 0, 0, 0, 3, 2, 1, 0),
    V("F7",    "Dom 7", "F A C Eb",  "Barre",    1, 3, 1, 2, 1, 1,   1, 3, 1, 2, 1, 1),
};
static const chord_variant_t C_DEG_V[] = {
    V("G",     "Major", "G B D",     "",        3, 2, 0, 0, 0, 3,   2, 1, 0, 0, 0, 3),
    V("Gmaj7", "Maj 7", "G B D F#",  "",        3, 2, 0, 0, 0, 2,   2, 1, 0, 0, 0, 3),
    V("G7",    "Dom 7", "G B D F",   "",        3, 2, 0, 0, 0, 1,   2, 1, 0, 0, 0, 1),
};
static const chord_variant_t C_DEG_VI[] = {
    V("Am",    "Minor", "A C E",     "",       -1, 0, 2, 2, 1, 0,   0, 0, 2, 3, 1, 0),
    V("Am7",   "Min 7", "A C E G",   "",       -1, 0, 2, 0, 1, 0,   0, 0, 2, 0, 1, 0),
    V("A7",    "Dom 7", "A C# E G",  "",       -1, 0, 2, 0, 2, 0,   0, 0, 2, 0, 2, 0),
};
static const chord_variant_t C_DEG_VII[] = {
    V("Bdim",  "Dim",   "B D F",     "",           -1, 2, 3, 4, 3, -1, 0, 1, 2, 4, 3, 0),
    V("Bm7b5", "m7b5",  "B D F A",   "Small barre",-1, 2, 3, 2, 3, -1, 0, 1, 3, 1, 3, 0),
};

/* ---------------- G 大调 ---------------- */

static const chord_variant_t G_DEG_I[] = {
    V("G",     "Major", "G B D",     "",        3, 2, 0, 0, 0, 3,   2, 1, 0, 0, 0, 3),
    V("Gmaj7", "Maj 7", "G B D F#",  "",        3, 2, 0, 0, 0, 2,   2, 1, 0, 0, 0, 3),
    V("G7",    "Dom 7", "G B D F",   "",        3, 2, 0, 0, 0, 1,   2, 1, 0, 0, 0, 1),
};
static const chord_variant_t G_DEG_II[] = {
    V("Am",    "Minor", "A C E",     "",       -1, 0, 2, 2, 1, 0,   0, 0, 2, 3, 1, 0),
    V("Am7",   "Min 7", "A C E G",   "",       -1, 0, 2, 0, 1, 0,   0, 0, 2, 0, 1, 0),
    V("A7",    "Dom 7", "A C# E G",  "",       -1, 0, 2, 0, 2, 0,   0, 0, 2, 0, 2, 0),
};
static const chord_variant_t G_DEG_III[] = {
    V("Bm",    "Minor", "B D F#",    "Barre",      -1, 2, 4, 4, 3, 2, 0, 1, 3, 4, 2, 1),
    V("Bm7",   "Min 7", "B D F# A",  "Easy grip",  -1, 2, 0, 2, 0, -1, 0, 1, 0, 1, 0, 0),
    V("B7",    "Dom 7", "B D# F# A", "",           -1, 2, 1, 2, 0, 2, 0, 2, 1, 3, 0, 4),
};
static const chord_variant_t G_DEG_IV[] = {
    V("C",     "Major", "C E G",     "",       -1, 3, 2, 0, 1, 0,   0, 3, 2, 0, 1, 0),
    V("Cmaj7", "Maj 7", "C E G B",   "",       -1, 3, 2, 0, 0, 0,   0, 3, 2, 0, 0, 0),
    V("C7",    "Dom 7", "C E G Bb",  "",       -1, 3, 2, 3, 1, 0,   0, 3, 2, 4, 1, 0),
};
static const chord_variant_t G_DEG_V[] = {
    V("D",     "Major", "D F# A",    "",       -1, -1, 0, 2, 3, 2, 0, 0, 0, 1, 3, 2),
    V("Dmaj7", "Maj 7", "D F# A C#", "Small barre",-1, -1, 0, 2, 2, 2, 0, 0, 0, 1, 1, 1),
    V("D7",    "Dom 7", "D F# A C",  "",       -1, -1, 0, 2, 1, 2, 0, 0, 0, 2, 1, 3),
};
static const chord_variant_t G_DEG_VI[] = {
    V("Em",    "Minor", "E G B",     "",        0, 2, 2, 0, 0, 0,   0, 2, 3, 0, 0, 0),
    V("Em7",   "Min 7", "E G B D",   "",        0, 2, 0, 0, 0, 0,   0, 2, 0, 0, 0, 0),
    V("E7",    "Dom 7", "E G# B D",  "",        0, 2, 0, 1, 0, 0,   0, 2, 0, 1, 0, 0),
};
static const chord_variant_t G_DEG_VII[] = {
    // F#dim 用常用转位(123 弦上的 A C F#), 无低音根音, 便于初学者按。
    V("F#dim",   "Dim",  "F# A C", "No low root", -1, -1, -1, 2, 1, 2, 0, 0, 0, 2, 1, 3),
    V("F#m7b5",  "m7b5", "F# A C E", "Small barre", 2, -1, 2, 2, 1, 0, 2, 0, 3, 3, 1, 0),
};

/* ---------------- 调与级数装配 ---------------- */

#define DEG(roman_, func_, table_) \
    { .roman = (roman_), .func = (func_), .variants = (table_), \
      .variant_count = sizeof(table_) / sizeof((table_)[0]) }

static const chord_degree_t C_DEGREES[CHORD_DEGREE_COUNT] = {
    DEG("I",    "Tonic",       C_DEG_I),
    DEG("ii",   "Supertonic",  C_DEG_II),
    DEG("iii",  "Mediant",     C_DEG_III),
    DEG("IV",   "Subdominant", C_DEG_IV),
    DEG("V",    "Dominant",    C_DEG_V),
    DEG("vi",   "Submediant",  C_DEG_VI),
    DEG("vii°", "Leading",     C_DEG_VII),
};

static const chord_degree_t G_DEGREES[CHORD_DEGREE_COUNT] = {
    DEG("I",    "Tonic",       G_DEG_I),
    DEG("ii",   "Supertonic",  G_DEG_II),
    DEG("iii",  "Mediant",     G_DEG_III),
    DEG("IV",   "Subdominant", G_DEG_IV),
    DEG("V",    "Dominant",    G_DEG_V),
    DEG("vi",   "Submediant",  G_DEG_VI),
    DEG("vii°", "Leading",     G_DEG_VII),
};

const chord_key_t CHORD_KEYS[CHORD_KEY_COUNT] = {
    { .name = "C Major", .degrees = C_DEGREES },
    { .name = "G Major", .degrees = G_DEGREES },
};

const chord_variant_t *chord_variant_get(int key, int degree, int variant)
{
    if (key < 0 || key >= CHORD_KEY_COUNT ||
        degree < 0 || degree >= CHORD_DEGREE_COUNT) {
        return NULL;
    }
    const chord_degree_t *deg = &CHORD_KEYS[key].degrees[degree];
    if (variant < 0 || variant >= deg->variant_count) {
        return NULL;
    }
    return &deg->variants[variant];
}

int chord_variant_count(int key, int degree)
{
    if (key < 0 || key >= CHORD_KEY_COUNT ||
        degree < 0 || degree >= CHORD_DEGREE_COUNT) {
        return 0;
    }
    return CHORD_KEYS[key].degrees[degree].variant_count;
}
