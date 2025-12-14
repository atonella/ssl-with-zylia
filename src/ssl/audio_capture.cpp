#include "audio_capture.h"
#include <cmath>
#include <cstring>
#include <iostream>

AudioCapture::AudioCapture(const MicrophoneConfig& config)
    : config_(config)
    , pcm_handle_(nullptr)
    , hw_params_(nullptr)
    , buffer_(nullptr)
    , is_running_(false)
    , is_initialized_(false)
{
    buffer_ = new int32_t[config_.period_size * config_.channels]; // Buffer of the application
}

AudioCapture::~AudioCapture()
{
    stop();

    if (hw_params_)
    {
        snd_pcm_hw_params_free(hw_params_);
        hw_params_ = nullptr;
    }

    if (pcm_handle_)
    {
        snd_pcm_close(pcm_handle_);
        pcm_handle_ = nullptr;
    }

    delete[] buffer_;
    buffer_ = nullptr;
}

bool AudioCapture::initialize()
{
    if (is_initialized_)
    {
        std::cerr << "Already initialized" << std::endl;
        return false;
    }

    // Open PCM device for capture
    int err = snd_pcm_open(&pcm_handle_, config_.device.c_str(), SND_PCM_STREAM_CAPTURE, 0);
    if (err < 0)
    {
        std::cerr << "Failed to open PCM device: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Configure hardware parameters
    if (!configure_hardware())
    {
        snd_pcm_close(pcm_handle_);
        pcm_handle_ = nullptr;
        return false;
    }

    is_initialized_ = true;
    return true;
}

bool AudioCapture::start()
{
    if (!is_initialized_)
    {
        std::cerr << "Not initialized. Call initialize() first." << std::endl;
        return false;
    }

    if (is_running_)
    {
        std::cerr << "Already running" << std::endl;
        return false;
    }

    // Prepares the PCM for IO after config
    int err = snd_pcm_prepare(pcm_handle_);
    if (err < 0)
    {
        std::cerr << "Cannot prepare microphone: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Explicitly starts the PCM stream
    err = snd_pcm_start(pcm_handle_);
    if (err < 0)
    {
        std::cerr << "Cannot start audio stream: " << snd_strerror(err) << std::endl;
        return false;
    }

    is_running_ = true;
    return true;
}

void AudioCapture::stop()
{
    std::cout << "Stopping audio capture ..." << std::endl;

    if (!is_running_)
    {
        return;
    }

    if (pcm_handle_)
    {
        int err = snd_pcm_drop(pcm_handle_);
        if (err < 0)
        {
            std::cerr << "Error stopping audio capture: " << snd_strerror(err) << std::endl;
        }
        else
        {
            is_running_ = false;
        }
    }
}

snd_pcm_sframes_t AudioCapture::read_audio(int32_t* buffer)
{
    if (!is_initialized_)
    {
        std::cerr << "Not initialized. Call initialize() first." << std::endl;
        return -1;
    }

    if (!is_running_)
    {
        std::cerr << "Not running. Call start() first." << std::endl;
        return -1;
    }

    // std::cout << "Capturing audio ..." << std::endl;

    snd_pcm_sframes_t frames = snd_pcm_readi(pcm_handle_, buffer, config_.period_size);

    if (frames == -EPIPE)
    {
        // Overrun occurred (buffer full)
        std::cerr << "WARNING: Buffer overrun occurred" << std::endl;
        snd_pcm_prepare(pcm_handle_); // Prepare PCM after overrun
        return frames;
    }
    else if (frames < 0)
    {
        std::cerr << "ERROR: " << snd_strerror(frames) << std::endl;
        return frames;
    }
    // else if (frames != static_cast<snd_pcm_sframes_t>(config_.period_size))
    // {
    //     std::cerr << "WARNING: Expected " << config_.period_size
    //               << " frames, but got " << frames << " frames)" << std::endl;
    // }

    return frames;
}

void AudioCapture::process_audio(AudioCallback callback, int num_iterations)
{
    if (!is_running_)
    {
        std::cerr << "Error: Not running. Call start() first." << std::endl;
        return;
    }

    int iteration = 0;
    while (num_iterations == 0 || iteration < num_iterations)
    {
        snd_pcm_sframes_t frames = read_audio(buffer_);

        if (frames > 0)
        {
            callback(buffer_, frames, config_.channels);
        }
        else if (frames != -EPIPE)
        {
            // Error other than overrun
            break;
        }
        // Continue on overrun (EPIPE)

        iteration++;
    }
}

/******************************
 *       PRIVATE METHODS      *
 ******************************/

bool AudioCapture::configure_hardware()
{
    int err;

    // Allocates the parameter container
    err = snd_pcm_hw_params_malloc(&hw_params_);
    if (err < 0)
    {
        std::cerr << "Cannot allocate hardware parameter structure: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Initialize with full default configuration
    err = snd_pcm_hw_params_any(pcm_handle_, hw_params_);
    if (err < 0)
    {
        std::cerr << "Cannot initialize hardware parameter structure: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Set access mode to interleaved (LRLR...)
    err = snd_pcm_hw_params_set_access(pcm_handle_, hw_params_, SND_PCM_ACCESS_RW_INTERLEAVED);
    if (err < 0)
    {
        std::cerr << "Cannot set interleaved access mode: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Set sample format (Signed 16-bit, Float, etc.)
    err = snd_pcm_hw_params_set_format(pcm_handle_, hw_params_, config_.format);
    if (err < 0)
    {
        std::cerr << "Cannot set sample format: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Set channel count
    err = snd_pcm_hw_params_set_channels(pcm_handle_, hw_params_, config_.channels);
    if (err < 0)
    {
        std::cerr << "Cannot set channel count: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Set sample rate
    unsigned int actual_rate = config_.sample_rate;
    // rounding direction of sample rate: -1 = accurate or first below, 0 = accurate, 1 = accurate or first above
    int dir = 0;
    err = snd_pcm_hw_params_set_rate_near(pcm_handle_, hw_params_, &actual_rate, &dir);
    if (err < 0)
    {
        std::cerr << "Cannot set sample rate: " << snd_strerror(err) << std::endl;
        return false;
    }
    if (actual_rate != config_.sample_rate)
    {
        // TODO: if this happens, we should save the actual value and use it
        std::cerr << "WARNING: Requested rate " << config_.sample_rate
                  << " Hz, but got " << actual_rate << " Hz" << std::endl;
    }

    // Set period size (interrupt interval)
    snd_pcm_uframes_t actual_period_size = config_.period_size;
    err = snd_pcm_hw_params_set_period_size_near(pcm_handle_, hw_params_, &actual_period_size, &dir);
    if (err < 0)
    {
        // TODO: if this happens, we should investigate it making it more safe
        std::cerr << "Cannot set period size: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Set buffer size of the hardware
    snd_pcm_uframes_t buffer_size = actual_period_size * 4; // multiple of period size, 2 .. 4 is common
    err = snd_pcm_hw_params_set_buffer_size_near(pcm_handle_, hw_params_, &buffer_size);
    if (err < 0)
    {
        // TODO: if this happens, we should investigate it making it more safe
        std::cerr << "Cannot set buffer size: " << snd_strerror(err) << std::endl;
        return false;
    }

    // Apply the hardware configuration
    err = snd_pcm_hw_params(pcm_handle_, hw_params_);
    if (err < 0)
    {
        std::cerr << "Cannot apply hardware parameters: " << snd_strerror(err) << std::endl;
        return false;
    }

    return true;
}
