/* SPDX-License-Identifier: MIT */
#include "nanotts_internal.h"

/*
 * The values below are an original, hand-tuned neutral starting voice. They
 * are intentionally compact rather than an acoustic copy of another engine.
 * Frequencies are in hertz. The end targets are used by diphthongs and glides.
 */
#define DEF(kind_, dur_, voice_, noise_, f1_, f2_, f3_, f4_, e1_, e2_, e3_, e4_, nc_, nb_, amp_) \
    { (uint8_t)(kind_), (uint8_t)(dur_), (uint8_t)(voice_), (uint8_t)(noise_), \
      (uint16_t)(f1_), (uint16_t)(f2_), (uint16_t)(f3_), (uint16_t)(f4_), \
      (uint16_t)(e1_), (uint16_t)(e2_), (uint16_t)(e3_), (uint16_t)(e4_), \
      (uint16_t)(nc_), (uint16_t)(nb_), (uint8_t)(amp_), 0u }

static const nanotts_phone_def_t PHONE_DEFS[NANOTTS_PH_COUNT] = {
    /* pauses */
    DEF(NANOTTS_KIND_PAUSE, 70, 0, 0, 500, 1500, 2500, 3500, 500, 1500, 2500, 3500, 0, 0, 0),
    DEF(NANOTTS_KIND_PAUSE, 28, 0, 0, 500, 1500, 2500, 3500, 500, 1500, 2500, 3500, 0, 0, 0),
    DEF(NANOTTS_KIND_PAUSE, 145, 0, 0, 500, 1500, 2500, 3500, 500, 1500, 2500, 3500, 0, 0, 0),

    /* vowels */
    DEF(NANOTTS_KIND_VOWEL, 105, 255, 4, 730, 1250, 2500, 3500, 730, 1250, 2500, 3500, 0, 0, 245), /* a */
    DEF(NANOTTS_KIND_VOWEL, 95, 255, 3, 430, 1950, 2620, 3500, 430, 1950, 2620, 3500, 0, 0, 238),  /* e */
    DEF(NANOTTS_KIND_VOWEL, 76, 245, 8, 500, 1500, 2500, 3450, 500, 1500, 2500, 3450, 0, 0, 215),  /* schwa */
    DEF(NANOTTS_KIND_VOWEL, 98, 255, 4, 610, 1760, 2490, 3400, 610, 1760, 2490, 3400, 0, 0, 242),  /* open e */
    DEF(NANOTTS_KIND_VOWEL, 92, 255, 2, 285, 2250, 3010, 3650, 285, 2250, 3010, 3650, 0, 0, 235),  /* i */
    DEF(NANOTTS_KIND_VOWEL, 82, 250, 3, 390, 2020, 2840, 3550, 390, 2020, 2840, 3550, 0, 0, 225),  /* small i */
    DEF(NANOTTS_KIND_VOWEL, 98, 255, 4, 450, 930, 2420, 3400, 450, 930, 2420, 3400, 0, 0, 242),    /* o */
    DEF(NANOTTS_KIND_VOWEL, 100, 255, 5, 570, 870, 2410, 3350, 570, 870, 2410, 3350, 0, 0, 242),   /* open o */
    DEF(NANOTTS_KIND_VOWEL, 94, 255, 3, 310, 900, 2250, 3300, 310, 900, 2250, 3300, 0, 0, 235),    /* u */
    DEF(NANOTTS_KIND_VOWEL, 84, 250, 4, 430, 1120, 2350, 3350, 430, 1120, 2350, 3350, 0, 0, 225),  /* small u */

    /* diphthongs */
    DEF(NANOTTS_KIND_DIPHTHONG, 145, 255, 3, 730, 1250, 2500, 3500, 300, 2200, 2980, 3650, 0, 0, 242), /* ai */
    DEF(NANOTTS_KIND_DIPHTHONG, 150, 255, 3, 730, 1250, 2500, 3500, 320, 900, 2250, 3300, 0, 0, 242),  /* au */
    DEF(NANOTTS_KIND_DIPHTHONG, 145, 255, 3, 450, 930, 2420, 3400, 300, 2200, 2980, 3650, 0, 0, 240),  /* oi */

    /* stops */
    DEF(NANOTTS_KIND_STOP_VOICELESS, 64, 0, 215, 500, 1200, 2500, 3500, 500, 1200, 2500, 3500, 3100, 2600, 225), /* p */
    DEF(NANOTTS_KIND_STOP_VOICED, 67, 115, 125, 300, 900, 2200, 3300, 300, 900, 2200, 3300, 1500, 1700, 220),   /* b */
    DEF(NANOTTS_KIND_STOP_VOICELESS, 62, 0, 225, 500, 1800, 2800, 3800, 500, 1800, 2800, 3800, 4900, 2800, 230),/* t */
    DEF(NANOTTS_KIND_STOP_VOICED, 66, 120, 125, 350, 1700, 2700, 3700, 350, 1700, 2700, 3700, 3700, 2200, 220), /* d */
    DEF(NANOTTS_KIND_STOP_VOICELESS, 70, 0, 225, 450, 1350, 2500, 3500, 450, 1350, 2500, 3500, 2100, 1800, 230),/* k */
    DEF(NANOTTS_KIND_STOP_VOICED, 72, 125, 120, 330, 1200, 2400, 3400, 330, 1200, 2400, 3400, 1750, 1500, 220), /* g */
    DEF(NANOTTS_KIND_STOP_VOICELESS, 62, 0, 70, 450, 1500, 2500, 3500, 450, 1500, 2500, 3500, 900, 900, 190),    /* glottal stop */

    /* affricates */
    DEF(NANOTTS_KIND_AFFRICATE_VOICELESS, 105, 0, 240, 420, 1700, 2800, 3800, 420, 1700, 2800, 3800, 3500, 2300, 238), /* ch */
    DEF(NANOTTS_KIND_AFFRICATE_VOICED, 110, 115, 205, 380, 1650, 2700, 3700, 380, 1650, 2700, 3700, 3000, 2200, 230),  /* j */

    /* fricatives */
    DEF(NANOTTS_KIND_FRICATIVE_VOICELESS, 88, 0, 190, 500, 1400, 2600, 3600, 500, 1400, 2600, 3600, 2500, 3000, 205), /* f */
    DEF(NANOTTS_KIND_FRICATIVE_VOICED, 90, 110, 150, 450, 1400, 2500, 3500, 450, 1400, 2500, 3500, 2400, 2800, 200),  /* v */
    DEF(NANOTTS_KIND_FRICATIVE_VOICELESS, 95, 0, 255, 450, 1800, 3200, 4500, 450, 1800, 3200, 4500, 5600, 2600, 245), /* s */
    DEF(NANOTTS_KIND_FRICATIVE_VOICED, 96, 110, 220, 430, 1750, 3100, 4400, 430, 1750, 3100, 4400, 5200, 2600, 235),  /* z */
    DEF(NANOTTS_KIND_FRICATIVE_VOICELESS, 94, 0, 205, 440, 1600, 2700, 3900, 440, 1600, 2700, 3900, 4300, 3200, 210), /* th */
    DEF(NANOTTS_KIND_FRICATIVE_VOICED, 95, 118, 145, 430, 1550, 2600, 3700, 430, 1550, 2600, 3700, 3600, 3000, 205),  /* dh */
    DEF(NANOTTS_KIND_FRICATIVE_VOICELESS, 100, 0, 250, 430, 1750, 2800, 3900, 430, 1750, 2800, 3900, 3400, 2300, 242),/* sh/ç */
    DEF(NANOTTS_KIND_FRICATIVE_VOICELESS, 92, 0, 225, 450, 1200, 2300, 3300, 450, 1200, 2300, 3300, 1750, 1700, 225),/* x */
    DEF(NANOTTS_KIND_FRICATIVE_VOICED, 94, 125, 150, 440, 1180, 2280, 3300, 440, 1180, 2280, 3300, 1650, 1750, 210),  /* gh */
    DEF(NANOTTS_KIND_FRICATIVE_VOICELESS, 72, 0, 155, 500, 1500, 2500, 3500, 500, 1500, 2500, 3500, 1200, 2500, 180),/* h */

    /* nasals */
    DEF(NANOTTS_KIND_NASAL, 84, 220, 8, 260, 900, 2200, 3200, 260, 900, 2200, 3200, 0, 0, 205),  /* m */
    DEF(NANOTTS_KIND_NASAL, 82, 220, 7, 270, 1500, 2450, 3400, 270, 1500, 2450, 3400, 0, 0, 205),/* n */
    DEF(NANOTTS_KIND_NASAL, 88, 220, 7, 280, 2050, 2850, 3650, 280, 2050, 2850, 3650, 0, 0, 205),/* ny */
    DEF(NANOTTS_KIND_NASAL, 88, 220, 7, 270, 1150, 2350, 3300, 270, 1150, 2350, 3300, 0, 0, 205),/* ng */

    /* liquids and glides */
    DEF(NANOTTS_KIND_LIQUID, 72, 235, 5, 360, 1300, 2600, 3500, 360, 1300, 2600, 3500, 0, 0, 215), /* l */
    DEF(NANOTTS_KIND_TRILL, 78, 225, 7, 420, 1450, 2500, 3450, 420, 1450, 2500, 3450, 0, 0, 215),   /* r */
    DEF(NANOTTS_KIND_TAP, 38, 215, 5, 420, 1450, 2500, 3450, 420, 1450, 2500, 3450, 0, 0, 205),     /* tap */
    DEF(NANOTTS_KIND_GLIDE, 64, 235, 4, 320, 850, 2250, 3300, 430, 1250, 2450, 3450, 0, 0, 205),    /* w */
    DEF(NANOTTS_KIND_GLIDE, 62, 235, 3, 300, 2200, 3000, 3650, 420, 1750, 2700, 3500, 0, 0, 205),   /* y */

    /* soft voiced bilabial approximant/fricative (Spanish beta allophone) */
    DEF(NANOTTS_KIND_FRICATIVE_VOICED, 82, 155, 72, 340, 920, 2200, 3300, 340, 920, 2200, 3300, 1150, 1800, 198) /* beta */
};

#undef DEF

static const char *const PHONE_NAMES[NANOTTS_PH_COUNT] = {
    "sil", "pause", "phrase",
    "a", "e", "schwa", "open-e", "i", "small-i", "o", "open-o", "u", "small-u", "ai", "au", "oi",
    "p", "b", "t", "d", "k", "g", "glottal-stop",
    "ch", "j",
    "f", "v", "s", "z", "th", "dh", "sh", "x", "gh", "h",
    "m", "n", "ny", "ng",
    "l", "r", "tap", "w", "y", "beta"
};

const nanotts_phone_def_t *nanotts_phone_def(nanotts_phone_t phone)
{
    if ((unsigned)phone >= (unsigned)NANOTTS_PH_COUNT) {
        return &PHONE_DEFS[NANOTTS_PH_SILENCE];
    }
    return &PHONE_DEFS[phone];
}

bool nanotts_phone_is_vowel(nanotts_phone_t phone)
{
    const nanotts_phone_def_t *def = nanotts_phone_def(phone);
    return def->kind == NANOTTS_KIND_VOWEL || def->kind == NANOTTS_KIND_DIPHTHONG;
}

bool nanotts_phone_is_pause(nanotts_phone_t phone)
{
    return nanotts_phone_def(phone)->kind == NANOTTS_KIND_PAUSE;
}

bool nanotts_phone_is_sonorant(nanotts_phone_t phone)
{
    nanotts_phone_kind_t kind = (nanotts_phone_kind_t)nanotts_phone_def(phone)->kind;
    return kind == NANOTTS_KIND_VOWEL || kind == NANOTTS_KIND_DIPHTHONG ||
           kind == NANOTTS_KIND_NASAL || kind == NANOTTS_KIND_LIQUID ||
           kind == NANOTTS_KIND_GLIDE || kind == NANOTTS_KIND_TRILL ||
           kind == NANOTTS_KIND_TAP;
}

const char *nanotts_phone_name(nanotts_phone_t phone)
{
    if ((unsigned)phone >= (unsigned)NANOTTS_PH_COUNT) {
        return "unknown";
    }
    return PHONE_NAMES[phone];
}
