
#include "alsa/asoundlib.h"


class SoundSourceLocalization {
private:
    // hw: no conversion, less configurable
    // plughw: software conversion, more configurable, automatic resampling
    const char *device_in_use = "plughw:2,0";
    const int mic_channels = 1;
    // Zylia uses 24 bit, Linux is LE (echo -n I | od -to2 | head -n1 | cut -f2 -d"
    // " | cut -c6 # 1 LE, 0 BE), signed is standard
    const snd_pcm_format_t mic_format = SND_PCM_FORMAT_FLOAT_LE;
    int dir = 0; // rounding direction of sample rate: -1 = accurate or first
    // bellow, 0 = accurate, 1 = accurate or first above
    unsigned int mic_sample_rate = 48000;
    snd_pcm_uframes_t mic_period_size = 1024; // in frames
    // latency = period_size / (sample_rate) * 1000 ms; with values from
    // above:: 21.33 ms
    snd_pcm_uframes_t mic_buffer_size = mic_period_size * 4;
    // Save a period to the buffer
    float *buffer;
public:
    SoundSourceLocalization();
    ~SoundSourceLocalization();
    int init_mic(snd_pcm_t *pcm_handle, snd_pcm_hw_params_t *&hw_params);
    int print_peak_volume();
};