/* SPDX-License-Identifier: MIT */
#include "nanotts/nanotts.h"

#include <assert.h>
#include <stddef.h>
#include <stdint.h>

static int contains(const nanotts_t *tts, nanotts_phone_t phone)
{
    const nanotts_event_t *events = nanotts_events(tts);
    size_t count = nanotts_event_count(tts);
    size_t index;
    for (index = 0u; index < count; ++index) {
        if (events[index].phone == (uint8_t)phone) return 1;
    }
    return 0;
}

static void expect_phone(const nanotts_t *tts, size_t index, nanotts_phone_t phone)
{
    assert(index < nanotts_event_count(tts));
    assert(nanotts_events(tts)[index].phone == (uint8_t)phone);
}

int main(void)
{
    nanotts_t tts;
    nanotts_parse_info_t info;
    const nanotts_event_t *events;
    const char malformed[] = { 'a', '_', (char)0xc0, (char)0x80 };
    nanotts_result_t result;

    assert(nanotts_init(&tts, 16000u, NANOTTS_LANG_NONE) == NANOTTS_OK);

    result = nanotts_parse_ipa(
        &tts, " s_ə_l_ˈa_m_a_t p_ˈa_ɡ_i", NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_OK);
    assert(info.error_byte == NANOTTS_NPOS);
    assert(nanotts_event_count(&tts) == 12u);
    events = nanotts_events(&tts);
    expect_phone(&tts, 0u, NANOTTS_PH_S);
    expect_phone(&tts, 1u, NANOTTS_PH_SCHWA);
    expect_phone(&tts, 3u, NANOTTS_PH_A);
    assert((events[3].flags & NANOTTS_EVENT_STRESS_PRIMARY) != 0u);
    expect_phone(&tts, 7u, NANOTTS_PH_SHORT_PAUSE);
    expect_phone(&tts, 10u, NANOTTS_PH_G);

    result = nanotts_parse_ipa(
        &tts,
        "a_e_ə_ɛ_i_ɪ_o_ɔ_u_ʊ_aɪ_aʊ_oɪ_"
        "p_b_t_d_k_ɡ_ʔ_t͡ʃ_d͡ʒ_f_v_s_z_θ_ð_ʃ_x_ɣ_h_"
        "m_n_ɲ_ŋ_l_r_ɾ_w_j",
        NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_OK);
    assert(nanotts_event_count(&tts) == 41u);
    assert(contains(&tts, NANOTTS_PH_SMALL_I));
    assert(contains(&tts, NANOTTS_PH_SMALL_U));
    assert(contains(&tts, NANOTTS_PH_GLOTTAL_STOP));
    assert(contains(&tts, NANOTTS_PH_TAP));

    result = nanotts_parse_ipa(
        &tts, "ɐ_ɜ_ɝ_q_g_ɣ_ɸ_ç_ɕ_χ_ɦ_ń_ʀ_ʋ_ɥ_ɟ_ʒ",
        NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_OK);
    assert(nanotts_event_count(&tts) == 17u);
    expect_phone(&tts, 0u, NANOTTS_PH_A);
    expect_phone(&tts, 1u, NANOTTS_PH_SCHWA);
    expect_phone(&tts, 3u, NANOTTS_PH_K);
    expect_phone(&tts, 5u, NANOTTS_PH_GH);
    expect_phone(&tts, 6u, NANOTTS_PH_F);
    expect_phone(&tts, 7u, NANOTTS_PH_SH);
    expect_phone(&tts, 9u, NANOTTS_PH_X);
    expect_phone(&tts, 11u, NANOTTS_PH_NY);
    expect_phone(&tts, 15u, NANOTTS_PH_J);

    result = nanotts_parse_ipa(
        &tts, "ɲ_ˈa_ɲ_i d_ˈɛ_ŋ_a_n ç_ˈa_r_a_t x_ˈu_s_u_s",
        NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_NY));
    assert(contains(&tts, NANOTTS_PH_NG));
    assert(contains(&tts, NANOTTS_PH_OPEN_E));
    assert(contains(&tts, NANOTTS_PH_SH));
    assert(contains(&tts, NANOTTS_PH_X));

    result = nanotts_parse_ipa(
        &tts, "t‍ʃ_a d͜ʒ_a b_ˈaɪ̯_k p_ˈu_l_aʊ̯",
        NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_CH));
    assert(contains(&tts, NANOTTS_PH_J));
    assert(contains(&tts, NANOTTS_PH_AI));
    assert(contains(&tts, NANOTTS_PH_AU));


    result = nanotts_parse_ipa(&tts, "ˈm̩_t_i", NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    assert(events[0].phone == (uint8_t)NANOTTS_PH_M);
    assert((events[0].flags & NANOTTS_EVENT_SYLLABIC) != 0u);
    assert((events[0].flags & NANOTTS_EVENT_STRESS_PRIMARY) != 0u);
    assert((events[2].flags & NANOTTS_EVENT_STRESS_PRIMARY) == 0u);

    result = nanotts_parse_ipa(
        &tts, "h_a_b_ˈa_r_i θ_ˌe_l_a_θ_ˈi_n_i ð_a_h_ˈa_b_u ɣ_ˈa_l_i",
        NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_TH));
    assert(contains(&tts, NANOTTS_PH_DH));
    assert(contains(&tts, NANOTTS_PH_GH));

    result = nanotts_parse_ipa(&tts, "β_ʝ_ʎ", NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_OK);
    expect_phone(&tts, 0u, NANOTTS_PH_BETA);
    expect_phone(&tts, 1u, NANOTTS_PH_Y);
    expect_phone(&tts, 2u, NANOTTS_PH_Y);

    result = nanotts_parse_ipa(&tts, "ˈs_aː", NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    expect_phone(&tts, 0u, NANOTTS_PH_S);
    expect_phone(&tts, 1u, NANOTTS_PH_A);
    assert((events[1].flags & NANOTTS_EVENT_STRESS_PRIMARY) != 0u);
    assert((events[1].flags & NANOTTS_EVENT_LONG) != 0u);
    assert(events[1].duration_percent == 150u);

    result = nanotts_parse_ipa(&tts, "a i", NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_OK);
    assert(nanotts_event_count(&tts) == 3u);
    expect_phone(&tts, 1u, NANOTTS_PH_SHORT_PAUSE);
    result = nanotts_parse_ipa(&tts, "a i", NANOTTS_NPOS,
                             NANOTTS_OPT_NO_AUTOPAUSE, &info);
    assert(result == NANOTTS_OK);
    assert(nanotts_event_count(&tts) == 2u);

    result = nanotts_parse_ipa(&tts, "a, b. k", NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_OK);
    assert(contains(&tts, NANOTTS_PH_SHORT_PAUSE));
    assert(contains(&tts, NANOTTS_PH_PHRASE_PAUSE));


    result = nanotts_parse_ipa(&tts, "a b, k?", NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_OK);
    events = nanotts_events(&tts);
    assert(events[1].phone == (uint8_t)NANOTTS_PH_SHORT_PAUSE);
    assert(events[1].duration_percent == 50u);
    assert(events[3].phone == (uint8_t)NANOTTS_PH_SHORT_PAUSE);
    assert(events[3].duration_percent == 165u);
    assert((events[nanotts_event_count(&tts) - 1u].flags &
            NANOTTS_EVENT_QUESTION) != 0u);

    result = nanotts_parse_ipa(&tts, "a_ʙ", NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_ERR_UNKNOWN_IPA);
    assert(info.error_byte != NANOTTS_NPOS);
    assert(info.error_codepoint == 0x0299u);
    result = nanotts_parse_ipa(&tts, "a_ʙ", NANOTTS_NPOS,
                             NANOTTS_OPT_PERMISSIVE_IPA, &info);
    assert(result == NANOTTS_OK);
    assert(nanotts_event_count(&tts) == 1u);

    result = nanotts_parse_ipa(&tts, malformed, sizeof(malformed), 0u, &info);
    assert(result == NANOTTS_ERR_UTF8);
    assert(info.error_byte == 2u);

    result = nanotts_parse_ipa(&tts, "___   ", NANOTTS_NPOS, 0u, &info);
    assert(result == NANOTTS_ERR_EMPTY_INPUT);
    return 0;
}
