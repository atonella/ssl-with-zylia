#pragma once

#include <alsa/asoundlib.h>
#include <cstdint>
#include <vector>

/**
 * @brief Calculate peak amplitudes for all channels
 * @param data Interleaved audio data
 * @param frames Number of frames
 * @param channels Number of channels
 * @param format Sample format (SND_PCM_FORMAT_S16_LE, ...)
 * @return Vector of peak amplitudes per channel
 */
std::vector<int32_t> calculate_peak_amplitudes(
    const int32_t* data, size_t frames, int channels, snd_pcm_format_t format);

/**
 * @brief Convert amplitude to decibels
 * @param amplitude Raw amplitude value
 * @param format Sample format (SND_PCM_FORMAT_S16_LE or SND_PCM_FORMAT_S24_LE)
 * @return Amplitude in dB (clamped to -60 dB minimum)
 */
float amplitude_to_db(int32_t amplitude, snd_pcm_format_t format);
