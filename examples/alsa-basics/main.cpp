#include "alsa/asoundlib.h"
#include <iostream>

// hw: no conversion, less configurable
// plughw: software conversion, more configurable, automatic resampling
const char* device_in_use = "plughw:3,0";
const int mic_channels = 1;
// Zylia uses 24 bit, Linux is LE (echo -n I | od -to2 | head -n1 | cut -f2 -d"
// " | cut -c6 # 1 LE, 0 BE), signed is standard
const snd_pcm_format_t mic_format = SND_PCM_FORMAT_S24_LE;
int dir = 0; // rounding direction of sample rate: -1 = accurate or first
             // bellow, 0 = accurate, 1 = accurate or first above
unsigned int mic_sample_rate = 48000;
snd_pcm_uframes_t mic_period_size = 1024; // in frames
snd_pcm_uframes_t mic_buffer_size = mic_period_size * 4;
// latency = period_size / (sample_rate) * 1000 ms; with values from
// above:: 21.33 ms

int init_mic(snd_pcm_t* pcm_handle, snd_pcm_hw_params_t*& hw_params)
{
    int err;
    bool success = true;
    int debug = 0;
    snd_pcm_hw_params_malloc(&hw_params); // Allocates the parameter container
    snd_pcm_hw_params_any(pcm_handle, hw_params); // Init with full default configuration
    snd_pcm_hw_params_set_access(
        pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED); // Access mode:
                                                               // SND_PCM_ACCESS_RW_INTERLEAVED (LRLR;
                                                               // standard) or
                                                               // SND_PCM_ACCESS_RW_NONINTERLEAVED (LLRR)
    snd_pcm_hw_params_set_format(pcm_handle, hw_params,
        mic_format); // Sets sample format (Signed 16-bit, Float, etc.).
    snd_pcm_hw_params_set_channels(pcm_handle, hw_params, mic_channels);
    snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &mic_sample_rate, &dir);
    snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &mic_period_size,
        &dir); // Sets interrupt interval size.
    snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hw_params, &mic_buffer_size); // Sets total ring buffer size.
    err = snd_pcm_hw_params(pcm_handle, hw_params); // Applies the configuration
    if (err) {
        std::cout << "Error setting HW params: " << snd_strerror(err) << std::endl;
        success = false;
    }
    return success;
}

int main()
{
    std::cout << "Starting ALSA test program..." << std::endl;
    std::cout << snd_pcm_format_width(mic_format) << std::endl;
    snd_pcm_t* pcm_handle = nullptr; // Handle for the PCM audio stream of a sound device
    snd_pcm_hw_params_t* hw_params = nullptr;
    snd_ctl_t* ctl_handle = nullptr; // Handle for the control interface (card capabilities).
    snd_pcm_stream_t stream;

    // SND_PCM_STREAM_PLAYBACK (Output) or SND_PCM_STREAM_CAPTURE (Input)
    if (snd_pcm_open(&pcm_handle, device_in_use, SND_PCM_STREAM_CAPTURE, 0)) {
        std::cout << "Error opening PCM device " << device_in_use << std::endl;
        return -1;
    }
    if (init_mic(pcm_handle, hw_params)) {
        snd_pcm_prepare(pcm_handle); //  Prepares the PCM for IO after config or overrun
        snd_pcm_start(pcm_handle); // Explicitly starts the PCM

        // Save a period to the buffer
        int32_t* buffer = new int32_t[mic_period_size * mic_channels];

        snd_pcm_sframes_t rc = snd_pcm_readi(pcm_handle, buffer, mic_period_size);
        if (rc == -EPIPE) {
            // Buffer full
            std::cout << "Overrun occurred." << std::endl;
            snd_pcm_prepare(pcm_handle); // Prepares the PCM for IO after an overrun
        } else if (rc < 0) {
            std::cout << "Error during snd_pcm_readi: " << snd_strerror(rc) << std::endl;
        } else if (rc != (snd_pcm_sframes_t)mic_period_size) {
            std::cout << "Read frames do not match expected." << std::endl
                      << "  read: " << rc << std::endl
                      << "  expected: " << mic_period_size << std::endl;
        } else {
            std::cout << "Read " << rc << " frames successfully." << std::endl;
        }

        snd_pcm_drop(pcm_handle); // stops stream; drops remaining data
        snd_pcm_close(pcm_handle); // Close PCM, free resources
    } else {
        std::cout << "Failed to initialize microphone." << std::endl;
        return -1;
    }

    std::cout << "ALSA test program finished successful." << std::endl;
    return 0;
}
