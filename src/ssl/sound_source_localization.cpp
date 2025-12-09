#include "sound_source_localization.h"
#include "../../../Spatial_Audio_Framework/framework/include/saf.h"
#include <iostream>
#include <cmath> // Add this at the top for std::abs


SoundSourceLocalization::SoundSourceLocalization()
{
    // Allocate buffer for audio data
    buffer = new float[mic_period_size * mic_channels];
}

SoundSourceLocalization::~SoundSourceLocalization()
{
    delete[] buffer;
}


int SoundSourceLocalization::init_mic(snd_pcm_t *pcm_handle, snd_pcm_hw_params_t *&hw_params)
{
    int err;
    bool success = true;
    int debug = 0;
    snd_pcm_hw_params_malloc(&hw_params);         // Allocates the parameter container
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
                                            &dir);                                    // Sets interrupt interval size.
    snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hw_params, &mic_buffer_size); // Sets total ring buffer size.
    err = snd_pcm_hw_params(pcm_handle, hw_params);                                  // Applies the configuration
    if (err)
    {
        std::cout << "Error setting HW params: " << snd_strerror(err) << std::endl;
        success = false;
    }
    return success;
}

int SoundSourceLocalization::print_peak_volume()
{
    std::cout << "Starting ALSA test program..." << std::endl;
    std::cout << snd_pcm_format_width(mic_format) << std::endl;
    snd_pcm_t *pcm_handle = nullptr; // Handle for the PCM audio stream of a sound device
    snd_pcm_hw_params_t *hw_params = nullptr;
    snd_ctl_t *ctl_handle = nullptr; // Handle for the control interface (card capabilities).
    snd_pcm_stream_t stream;

    // SND_PCM_STREAM_PLAYBACK (Output) or SND_PCM_STREAM_CAPTURE (Input)
    if (snd_pcm_open(&pcm_handle, device_in_use, SND_PCM_STREAM_CAPTURE, 0))
    {
        std::cout << "Error opening PCM device " << device_in_use << std::endl;
        return -1;
    }
    if (this->init_mic(pcm_handle, hw_params))
    {
        snd_pcm_prepare(pcm_handle); //  Prepares the PCM for IO after config or overrun
        snd_pcm_start(pcm_handle);   // Explicitly starts the PCM


        // Loop to visualize audio data
        std::cout << "Capturing... (Make some noise!)" << std::endl;

        for (int i = 0; i < 500; ++i)
        {
            snd_pcm_sframes_t rc = snd_pcm_readi(pcm_handle, buffer, mic_period_size);

            if (rc == -EPIPE)
            {
                // Buffer full
                // std::cout << "Overrun occurred." << std::endl; // Commented out to reduce spam
                snd_pcm_prepare(pcm_handle);
            }
            else if (rc < 0)
            {
                std::cout << "Error: " << snd_strerror(rc) << std::endl;
            }
            else
            {
                // Calculate Peak Amplitude for Channel 1
                // Data is interleaved: [Ch1, Ch2... ChN, Ch1, Ch2...]
                float max_val = 0.0f;

                for (int f = 0; f < rc; ++f)
                {
                    // Access Channel 1 (index 0) of frame f
                    float sample = buffer[f * mic_channels + 0];

                    if (std::abs(sample) > max_val)
                    {
                        max_val = std::abs(sample);
                    }
                }

                // Draw VU Meter
                // For float format, max value is 1.0
                int bars = (int)(max_val * 50); // Scale to 50 chars width
                if (bars > 50)
                    bars = 50;

                std::cout << "Ch1 Level: [";
                for (int b = 0; b < bars; ++b)
                    std::cout << "#";
                for (int b = bars; b < 50; ++b)
                    std::cout << " ";
                std::cout << "] " << max_val << "\r" << std::flush;
            }
        }

        std::cout << std::endl
                    << "Capture finished." << std::endl;

        // This is now handled by the destructor
        // delete[] buffer; // Clean up memory

        snd_pcm_drop(pcm_handle);  // stops stream; drops remaining data
        snd_pcm_close(pcm_handle); // Close PCM, free resources
    }
    else
    {
        std::cout << "Failed to initialize microphone." << std::endl;
        return -1;
    }

    std::cout << "ALSA test program finished successful." << std::endl;
    return 0;
}


int main(){
    SoundSourceLocalization ssl;
    return ssl.print_peak_volume();
}