#pragma once

#include "microphone_config.h"

#include <alsa/asoundlib.h>
#include <functional>
#include <string>
#include <vector>

/**
 * @brief Sound Source Localization class for Zylia ZM-1 microphone array
 *
 * Provides audio capture from the Zylia ZM-1 19-channel microphone array
 * and supports real-time audio processing for sound source localization.
 */
class AudioCapture
{
public:
    // Audio callback function type: receives interleaved audio data, frame count, and channel count
    using AudioCallback = std::function<void(const int32_t* data, size_t frames, int channels)>;
    // TODO: eval if needed or make it simpler
    // EXPLANATION: AudioCallback is a type alias for a function that processes audio.
    // WHAT: A std::function that takes 3 parameters: pointer to audio samples, number of frames, number of channels
    // WHY: Allows users to pass lambda/functions to process_audio() for flexible processing (peak detection,
    //      beamforming, DOA estimation, etc.) without hardcoding the algorithm in the class.

    /**
     * @brief Constructor: reserves buffer
     */
    explicit AudioCapture(const MicrophoneConfig& config);

    /**
     * @brief Destructor: free resources, cleanup alsa handles
     */
    ~AudioCapture();

    // As ALSA handles are not copyable, disable copy constructor and assignment operator
    AudioCapture(const AudioCapture&) = delete;
    AudioCapture& operator=(const AudioCapture&) = delete;

    /**
     * @brief Initialize and open the microphone
     * @return true on success, false on failure
     */
    bool initialize();

    /**
     * @brief Start audio capture
     * @return true on success, false on failure
     */
    bool start();

    /**
     * @brief Stop audio capture
     */
    void stop();

    /**
     * @brief Read one period of audio data
     * @param buffer Application input buffer (must be pre-allocated with size = period_size * channels
     * @return Number of frames read, or negative error code
     */
    snd_pcm_sframes_t read_audio(int32_t* buffer);

    /**
     * @brief Process audio in a loop with a callback function
     * @param callback Function to process each audio period
     * @param num_iterations Number of periods to capture (0 = infinite)
     */
    void process_audio(AudioCallback callback, int num_iterations = 0);
    // TTODO: das ist echt krass mit dem lambda. aber maybe overkill and simplier is better
    // EXPLANATION: WHAT = High-level convenience method that repeatedly reads audio and calls your callback.
    // WHY = Instead of writing the read loop yourself, pass a callback (lambda/function) and this handles
    //       the loop. num_iterations=0 means infinite loop (until stop() called), useful for real-time apps.
    // EXAMPLE: ssl.process_audio([](const int32_t* data, size_t frames, int channels) {
    //              // Your processing code here (peak detection, beamforming, etc.)
    //          }, 100); // Process 100 periods then return

    /************************
     *       GETTERS        *
     ************************/

    /**
     * @brief Get current configuration
     */
    const MicrophoneConfig& get_config() const { return config_; }

    /**
     * @brief Check if currently capturing audio
     */
    bool is_running() const { return is_running_; }

private:
    /**
     * @brief Configure the microphone hardware parameters for ALSA
     * @return true on success, false on failure
     */
    bool configure_hardware();

    MicrophoneConfig config_;
    snd_pcm_t* pcm_handle_;
    snd_pcm_hw_params_t* hw_params_;
    int32_t* buffer_;
    bool is_running_;
    bool is_initialized_;
};
