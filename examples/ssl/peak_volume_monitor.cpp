#include "../../src/ssl/audio_capture.h"
#include "../../src/ssl/utils.h"
#include <iomanip>
#include <iostream>

/**
 * @brief Example: Monitor peak volume levels
 *
 * This example demonstrates basic usage of the AudioCapture class:
 * - Initialize the audio device
 * - Start capture
 * - Process audio with a callback function
 * - Display real-time VU meters for all channels
 */

void displayVUMeters(const std::vector<int32_t>& peaks, snd_pcm_format_t format)
{
    // Clear screen and move cursor to top
    std::cout << "\033[2J\033[H";

    const float min_db = -60.0f;

    for (size_t ch = 0; ch < peaks.size(); ++ch)
    {
        float db = amplitude_to_db(peaks[ch], format);

        // Map dB range [min_db, 0] to [0, 40] bars
        int bars = static_cast<int>(((db - min_db) / (-min_db)) * 40);
        if (bars < 0)
            bars = 0;
        if (bars > 40)
            bars = 40;

        std::cout << "Ch" << (ch + 1 < 10 ? " " : "") << (ch + 1) << " [";
        for (int b = 0; b < bars; ++b)
            std::cout << "#";
        for (int b = bars; b < 40; ++b)
            std::cout << " ";
        std::cout << "] " << std::fixed << std::setprecision(1)
                  << std::setw(6) << db << " dB" << std::endl;
    }
    std::cout << std::flush;
}

int main()
{
    std::cout << "Peak Volume Monitor" << std::endl;
    std::cout << "===================" << std::endl
              << std::endl;

    // Create SSL object
    AudioCapture mic(MIC_CFG_NEEWER_NW_7000);

    // Initialize the audio device
    if (!mic.initialize())
    {
        return 1;
    }

    std::cout << "Device initialized successfully" << std::endl;
    std::cout << "Configuration:" << std::endl;
    std::cout << "  Device: " << mic.get_config().device << std::endl;
    std::cout << "  Channels: " << mic.get_config().channels << std::endl;
    std::cout << "  Sample Rate: " << mic.get_config().sample_rate << " Hz" << std::endl;
    std::cout << "  Period Size: " << mic.get_config().period_size << " frames" << std::endl;
    std::cout << "  Latency: " << (mic.get_config().period_size * 1000.0 / mic.get_config().sample_rate)
              << " ms" << std::endl
              << std::endl;

    std::cout << "Starting capture... (Make some noise!)" << std::endl;
    std::cout << "Press Ctrl+C to stop" << std::endl
              << std::endl;

    // Start audio capture
    if (!mic.start())
    {
        return 1;
    }

    const snd_pcm_format_t format = mic.get_config().format;

    // Process audio with a callback function
    mic.process_audio([format](const int32_t* data, size_t frames, int channels)
        {
            // Calculate peak amplitudes for all channels
            auto peaks = calculate_peak_amplitudes(data, frames, channels, format);
            
            // Display VU meters
            displayVUMeters(peaks, format); },
        500); // Capture 500 periods (about 10 seconds)

    std::cout << std::endl
              << "Capture finished." << std::endl;

    return 0;
}
