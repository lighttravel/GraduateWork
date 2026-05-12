#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#define AUDIO_SERVICE_TEST
#include "../../main/services/audio/audio_service.c"

static void test_tone_samples_are_bounded(void)
{
    int16_t samples[64] = {0};

    audio_fill_square_wave(samples, 64, 12000);

    for (size_t i = 0; i < 64; ++i) {
        assert(samples[i] <= 12000);
        assert(samples[i] >= -12000);
    }
}

static void test_tone_samples_toggle_polarity(void)
{
    int16_t samples[64] = {0};
    bool has_positive = false;
    bool has_negative = false;

    audio_fill_square_wave(samples, 64, 8000);

    for (size_t i = 0; i < 64; ++i) {
        has_positive = has_positive || samples[i] > 0;
        has_negative = has_negative || samples[i] < 0;
    }

    assert(has_positive);
    assert(has_negative);
}

static void test_tone_rejects_invalid_input(void)
{
    audio_fill_square_wave(NULL, 64, 8000);
    audio_fill_square_wave((int16_t[1]){0}, 0, 8000);
}

int main(void)
{
    test_tone_samples_are_bounded();
    test_tone_samples_toggle_polarity();
    test_tone_rejects_invalid_input();
    puts("audio tone tests passed");
    return 0;
}
