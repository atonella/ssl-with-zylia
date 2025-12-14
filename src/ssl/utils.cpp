#include "utils.h"
#include <cmath>

/**
 * @brief Convert 24-bit sample to properly sign-extended 32-bit
 */
static inline int32_t fix_24_bit_sample(int32_t sample) // TODO: in labor ausprobieren, welche range an wertden das zylia liefert. dann evaluieren ob diese funktion notwendig ist
{
    return (sample << 8) >> 8;
}

/**
 * @brief Convert 16-bit sample to properly sign-extended 32-bit
 */
static inline int32_t fix_16_bit_sample(int32_t sample)
{
    return static_cast<int16_t>(sample);
}

/**
 * @brief Fix sample based on format
 */
static inline int32_t fix_sample(int32_t sample, snd_pcm_format_t format)
{
    switch (format)
    {
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S24_BE:
        case SND_PCM_FORMAT_S24_3LE:
        case SND_PCM_FORMAT_S24_3BE:
            return fix_24_bit_sample(sample);
        case SND_PCM_FORMAT_S16_LE:
        case SND_PCM_FORMAT_S16_BE:
            return fix_16_bit_sample(sample);
        default:
            // For other formats, assume no fix needed
            return sample;
    }
}

/**
 * @brief Get maximum amplitude value for the given format
 */
static inline float get_max_amplitude(snd_pcm_format_t format)
{
    switch (format)
    {
        case SND_PCM_FORMAT_S24_LE:
        case SND_PCM_FORMAT_S24_BE:
        case SND_PCM_FORMAT_S24_3LE:
        case SND_PCM_FORMAT_S24_3BE:
            return 8388608.0f; // 2^23
        case SND_PCM_FORMAT_S16_LE:
        case SND_PCM_FORMAT_S16_BE:
            return 32768.0f; // 2^15
        default:
            return 8388608.0f; // Default to 24-bit
    }
}

std::vector<int32_t> calculate_peak_amplitudes(
    const int32_t* data, size_t frames, int channels, snd_pcm_format_t format)
{
    std::vector<int32_t> peaks(channels, 0);

    for (size_t f = 0; f < frames; ++f)
    {
        for (int ch = 0; ch < channels; ++ch)
        {
            int32_t sample = data[f * channels + ch];
            sample = fix_sample(sample, format);

            int32_t abs_sample = std::abs(sample);
            if (abs_sample > peaks[ch])
            {
                peaks[ch] = abs_sample;
            }
        }
    }

    return peaks;
}

float amplitude_to_db(int32_t amplitude, snd_pcm_format_t format)
{
    const float max_amplitude = get_max_amplitude(format);
    const float min_db = -60.0f;

    if (amplitude <= 0)
    {
        return min_db;
    }

    float db = 20.0f * log10f(static_cast<float>(amplitude) / max_amplitude);
    return (db < min_db) ? min_db : db;
}
