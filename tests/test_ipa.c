/* SPDX-License-Identifier: MIT */
#include "idtts/idtts.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static int contains(const idtts_t *tts, idtts_phone_t phone)
{
    const idtts_event_t *events = idtts_events(tts);
    size_t count = idtts_event_count(tts);
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (events[index].phone == (uint8_t)phone) return 1;
    }
    return 0;
}

static void expect_phone(const idtts_t *tts, size_t index, idtts_phone_t phone)
{
    assert(index < idtts_event_count(tts));
    assert(idtts_events(tts)[index].phone == (uint8_t)phone);
}

int main(void)
{
    idtts_t tts;
    idtts_parse_info_t info;
    const idtts_event_t *events;
    const char malformed[] = { 'a', '_', (char)0xc0, (char)0x80 };
    idtts_result_t result;

    assert(idtts_init(&tts, 16000u) == IDTTS_OK);

    result = idtts_parse_ipa(
        &tts, " s_ə_l_ˈa_m_a_t p_ˈa_ɡ_i", IDTTS_NPOS, 0u, &info);
    assert(result == IDTTS_OK);
    assert(info.error_byte == IDTTS_NPOS);
    assert(idtts_event_count(&tts) == 12u);
    events = idtts_events(&tts);
    expect_phone(&tts, 0u, IDTTS_PH_S);
    expect_phone(&tts, 1u, IDTTS_PH_SCHWA);
    expect_phone(&tts, 3u, IDTTS_PH_A);
    assert((events[3].flags & IDTTS_EVENT_STRESS_PRIMARY) != 0u);
    expect_phone(&tts, 7u, IDTTS_PH_SHORT_PAUSE);
    expect_phone(&tts, 10u, IDTTS_PH_G);

    result = idtts_parse_ipa(
        &tts,
        "a_e_ə_ɛ_i_ɪ_o_ɔ_u_ʊ_aɪ_aʊ_oɪ_"
        "p_b_t_d_k_ɡ_ʔ_t͡ʃ_d͡ʒ_f_v_s_z_ʃ_x_h_"
        "m_n_ɲ_ŋ_l_r_ɾ_w_j",
        IDTTS_NPOS, 0u, &info);
    assert(result == IDTTS_OK);
    assert(idtts_event_count(&tts) == 38u);
    assert(contains(&tts, IDTTS_PH_SMALL_I));
    assert(contains(&tts, IDTTS_PH_SMALL_U));
    assert(contains(&tts, IDTTS_PH_GLOTTAL_STOP));
    assert(contains(&tts, IDTTS_PH_TAP));

    result = idtts_parse_ipa(
        &tts, "ɐ_ɜ_ɝ_q_g_ɣ_ɸ_ç_ɕ_χ_ɦ_ń_ʀ_ʋ_ɥ_ɟ_ʒ",
        IDTTS_NPOS, 0u, &info);
    assert(result == IDTTS_OK);
    assert(idtts_event_count(&tts) == 17u);
    expect_phone(&tts, 0u, IDTTS_PH_A);
    expect_phone(&tts, 1u, IDTTS_PH_SCHWA);
    expect_phone(&tts, 3u, IDTTS_PH_K);
    expect_phone(&tts, 6u, IDTTS_PH_F);
    expect_phone(&tts, 7u, IDTTS_PH_SH);
    expect_phone(&tts, 9u, IDTTS_PH_X);
    expect_phone(&tts, 11u, IDTTS_PH_NY);
    expect_phone(&tts, 15u, IDTTS_PH_J);

    result = idtts_parse_ipa(
        &tts, "ɲ_ˈa_ɲ_i d_ˈɛ_ŋ_a_n ç_ˈa_r_a_t x_ˈu_s_u_s",
        IDTTS_NPOS, 0u, &info);
    assert(result == IDTTS_OK);
    assert(contains(&tts, IDTTS_PH_NY));
    assert(contains(&tts, IDTTS_PH_NG));
    assert(contains(&tts, IDTTS_PH_OPEN_E));
    assert(contains(&tts, IDTTS_PH_SH));
    assert(contains(&tts, IDTTS_PH_X));

    result = idtts_parse_ipa(
        &tts, "t‍ʃ_a d͜ʒ_a b_ˈaɪ̯_k p_ˈu_l_aʊ̯",
        IDTTS_NPOS, 0u, &info);
    assert(result == IDTTS_OK);
    assert(contains(&tts, IDTTS_PH_CH));
    assert(contains(&tts, IDTTS_PH_J));
    assert(contains(&tts, IDTTS_PH_AI));
    assert(contains(&tts, IDTTS_PH_AU));

    result = idtts_parse_ipa(&tts, "ˈs_aː", IDTTS_NPOS, 0u, &info);
    assert(result == IDTTS_OK);
    events = idtts_events(&tts);
    expect_phone(&tts, 0u, IDTTS_PH_S);
    expect_phone(&tts, 1u, IDTTS_PH_A);
    assert((events[1].flags & IDTTS_EVENT_STRESS_PRIMARY) != 0u);
    assert((events[1].flags & IDTTS_EVENT_LONG) != 0u);
    assert(events[1].duration_percent == 150u);

    result = idtts_parse_ipa(&tts, "a i", IDTTS_NPOS, 0u, &info);
    assert(result == IDTTS_OK);
    assert(idtts_event_count(&tts) == 3u);
    expect_phone(&tts, 1u, IDTTS_PH_SHORT_PAUSE);
    result = idtts_parse_ipa(&tts, "a i", IDTTS_NPOS,
                             IDTTS_OPT_NO_AUTOPAUSE, &info);
    assert(result == IDTTS_OK);
    assert(idtts_event_count(&tts) == 2u);

    result = idtts_parse_ipa(&tts, "a, b. k", IDTTS_NPOS, 0u, &info);
    assert(result == IDTTS_OK);
    assert(contains(&tts, IDTTS_PH_SHORT_PAUSE));
    assert(contains(&tts, IDTTS_PH_PHRASE_PAUSE));


    result = idtts_parse_ipa(&tts, "a b, k?", IDTTS_NPOS, 0u, &info);
    assert(result == IDTTS_OK);
    events = idtts_events(&tts);
    assert(events[1].phone == (uint8_t)IDTTS_PH_SHORT_PAUSE);
    assert(events[1].duration_percent == 50u);
    assert(events[3].phone == (uint8_t)IDTTS_PH_SHORT_PAUSE);
    assert(events[3].duration_percent == 165u);
    assert((events[idtts_event_count(&tts) - 1u].flags &
            IDTTS_EVENT_QUESTION) != 0u);

    result = idtts_parse_ipa(&tts, "a_θ", IDTTS_NPOS, 0u, &info);
    assert(result == IDTTS_ERR_UNKNOWN_IPA);
    assert(info.error_byte != IDTTS_NPOS);
    assert(info.error_codepoint == 0x03b8u);
    result = idtts_parse_ipa(&tts, "a_θ", IDTTS_NPOS,
                             IDTTS_OPT_PERMISSIVE_IPA, &info);
    assert(result == IDTTS_OK);
    assert(idtts_event_count(&tts) == 1u);

    result = idtts_parse_ipa(&tts, malformed, sizeof(malformed), 0u, &info);
    assert(result == IDTTS_ERR_UTF8);
    assert(info.error_byte == 2u);

    result = idtts_parse_ipa(&tts, "___   ", IDTTS_NPOS, 0u, &info);
    assert(result == IDTTS_ERR_EMPTY_INPUT);
    return 0;
}
