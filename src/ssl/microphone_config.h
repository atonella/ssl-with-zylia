#pragma once

#include <alsa/asoundlib.h>
#include <string>

/**
 * @brief Configuration parameters of microphone in use
 */
struct MicrophoneConfig
{
    std::string device; // ALSA device name (see `arecord -l` for name on your machine)
    int channels;
    unsigned int sample_rate;
    snd_pcm_uframes_t period_size; // Period size in frames; latency = period_size / (sample_rate) * 1000 ms;
    snd_pcm_format_t format;

    MicrophoneConfig(std::string device, int channels, unsigned int sample_rate,
        snd_pcm_uframes_t period_size, snd_pcm_format_t format)
        : device(device)
        , channels(channels)
        , sample_rate(sample_rate)
        , period_size(period_size)
        , format(format)
    {
    }
};

// TODO: If overruns occur, increase the period size
// plughw: conversions of format possible
// dsnoop: no conversion, multiple apps can use the device simultaneously
const MicrophoneConfig MIC_CFG_ZYLIA_ZM_1("plughw:2,0", 19, 48000, 1024, SND_PCM_FORMAT_S24_LE);
// const MicrophoneConfig MIC_CFG_NEEWER_NW_7000("dsnoop:CARD=Device,DEV=0", 1, 44100, 1024, SND_PCM_FORMAT_S16_LE);
const MicrophoneConfig MIC_CFG_NEEWER_NW_7000("plughw:3,0", 1, 48000, 1024, SND_PCM_FORMAT_S16_LE);
