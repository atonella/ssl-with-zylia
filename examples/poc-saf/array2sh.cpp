#include <iostream>
#include <iomanip>
#include <cmath>
#include <cstring>
#include "alsa/asoundlib.h"

// SAF framework includes
#include "saf.h"
#include "array2sh.h"
#include "sldoa.h"

// ALSA Configuration
const char* device_in_use = "plughw:2,0";
const int mic_channels = 19;
const snd_pcm_format_t mic_format = SND_PCM_FORMAT_S24_LE;
int dir = 0;
unsigned int mic_sample_rate = 48000;
snd_pcm_uframes_t mic_period_size = 128; // Match SAF frame size
snd_pcm_uframes_t mic_buffer_size = mic_period_size * 8;

// SAF Configuration
const int SH_ORDER = 3; // Zylia supports up to 3rd order
const int NUM_SH_SIGNALS = (SH_ORDER + 1) * (SH_ORDER + 1); // 16 SH channels

int init_mic(snd_pcm_t* pcm_handle, snd_pcm_hw_params_t*& hw_params)
{
    int err;
    bool success = true;
    
    snd_pcm_hw_params_malloc(&hw_params);
    snd_pcm_hw_params_any(pcm_handle, hw_params);
    snd_pcm_hw_params_set_access(pcm_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(pcm_handle, hw_params, mic_format);
    snd_pcm_hw_params_set_channels(pcm_handle, hw_params, mic_channels);
    snd_pcm_hw_params_set_rate_near(pcm_handle, hw_params, &mic_sample_rate, &dir);
    snd_pcm_hw_params_set_period_size_near(pcm_handle, hw_params, &mic_period_size, &dir);
    snd_pcm_hw_params_set_buffer_size_near(pcm_handle, hw_params, &mic_buffer_size);
    
    err = snd_pcm_hw_params(pcm_handle, hw_params);
    if (err) {
        std::cout << "Error setting HW params: " << snd_strerror(err) << std::endl;
        success = false;
    }
    return success;
}

// Convert interleaved 24-bit samples to float arrays for SAF (channel-major)
void convert_interleaved_to_float_channels(int32_t* interleaved, float** channels, 
                                            int num_frames, int num_channels)
{
    const float scale = 1.0f / 8388608.0f; // 2^23 for 24-bit normalization
    
    for (int f = 0; f < num_frames; ++f) {
        for (int ch = 0; ch < num_channels; ++ch) {
            int32_t sample = interleaved[f * num_channels + ch];
            // Sign extension for 24-bit in 32-bit container
            sample = (sample << 8) >> 8;
            channels[ch][f] = (float)sample * scale;
        }
    }
}

int main()
{
    std::cout << "=== SAF Ambisonics POC ===" << std::endl;
    std::cout << "Microphone channels: " << mic_channels << std::endl;
    std::cout << "SH Order: " << SH_ORDER << " (" << NUM_SH_SIGNALS << " SH signals)" << std::endl;
    
    // === Initialize SAF components ===
    
    // 1. Create array2sh instance (microphone array to spherical harmonics)
    void* array2sh_handle = nullptr;
    array2sh_create(&array2sh_handle);
    array2sh_init(array2sh_handle, mic_sample_rate);
    
    // Configure for Zylia ZM-1 (19 microphones on a sphere)
    array2sh_setPreset(array2sh_handle, MICROPHONE_ARRAY_PRESET_ZYLIA_1D);
    array2sh_setEncodingOrder(array2sh_handle, (SH_ORDERS)SH_ORDER);
    array2sh_setNormType(array2sh_handle, NORM_SN3D);
    array2sh_setChOrder(array2sh_handle, CH_ACN);
    //array2sh_setGain(array2sh_handle, 30.0f);

    // Evaluate encoder (computes encoding filters)
    std::cout << "Initializing array2sh encoder..." << std::endl;
    array2sh_evalEncoder(array2sh_handle);
    
    // Wait for initialization to complete
    while (array2sh_getEvalStatus(array2sh_handle) == EVAL_STATUS_EVALUATING) {
        std::cout << "." << std::flush;
        usleep(100000); // 100ms
    }
    std::cout << " Done!" << std::endl;
    
    // 2. Create sldoa instance (spatial localization based on direction of arrival)
    void* sld_handle = nullptr;
    sldoa_create(&sld_handle);
    sldoa_init(sld_handle, mic_sample_rate);
    
    // Configure sldoa
    sldoa_setMasterOrder(sld_handle, (SH_ORDERS)SH_ORDER);
    sldoa_setNormType(sld_handle, NORM_SN3D);
    sldoa_setChOrder(sld_handle, CH_ACN);
    
    // CRITICAL: Initialize the codec - without this, sldoa_analysis does nothing!
    std::cout << "Initializing sldoa codec..." << std::endl;
    sldoa_initCodec(sld_handle);
    while (sldoa_getCodecStatus(sld_handle) == CODEC_STATUS_INITIALISING) {
        std::cout << "." << std::flush;
        usleep(100000); // 100ms
    }
    std::cout << " Done!" << std::endl;
    
    // Get frame sizes
    int a2sh_framesize = array2sh_getFrameSize();
    int sldoa_framesize = sldoa_getFrameSize();
    
    std::cout << "array2sh frame size: " << a2sh_framesize << std::endl;
    std::cout << "sldoa frame size: " << sldoa_framesize << std::endl;
    
    // Use array2sh frame size for ALSA (smaller, more responsive)
    // We'll accumulate frames for sldoa internally
    int framesize = a2sh_framesize;  // 128 samples
    mic_period_size = framesize;
    
    // sldoa processes every SLDOA_FRAME_SIZE (512) samples
    // It has 4 time slots (512/128 = 4), so we should only read display data
    // after 4 frames have been processed
    int frames_per_sldoa_update = sldoa_framesize / a2sh_framesize; // 512/128 = 4
    int frame_counter = 0;
    
    // === Allocate audio buffers ===
    
    // Input buffer for ALSA (interleaved 24-bit)
    int32_t* alsa_buffer = new int32_t[framesize * mic_channels];
    
    // Input channels for array2sh (float, channel-major)
    float** mic_input = new float*[mic_channels];
    for (int i = 0; i < mic_channels; ++i) {
        mic_input[i] = new float[framesize];
    }
    
    // Output SH signals from array2sh
    float** sh_output = new float*[NUM_SH_SIGNALS];
    for (int i = 0; i < NUM_SH_SIGNALS; ++i) {
        sh_output[i] = new float[framesize];
    }
    
    // === Initialize ALSA ===
    snd_pcm_t* pcm_handle = nullptr;
    snd_pcm_hw_params_t* hw_params = nullptr;
    
    if (snd_pcm_open(&pcm_handle, device_in_use, SND_PCM_STREAM_CAPTURE, 0)) {
        std::cout << "Error opening PCM device " << device_in_use << std::endl;
        return -1;
    }
    
    if (!init_mic(pcm_handle, hw_params)) {
        std::cout << "Failed to initialize microphone." << std::endl;
        return -1;
    }
    
    snd_pcm_prepare(pcm_handle);
    snd_pcm_start(pcm_handle);
    
    std::cout << "\n=== Capturing and Processing ===" << std::endl;
    std::cout << "Make some noise! (Clap, snap, speak...)" << std::endl;
    std::cout << "Press Ctrl+C to exit.\n" << std::endl;
    
    // Persistent display data (only update when sldoa produces new output)
    float* azi_deg = nullptr;
    float* elev_deg = nullptr;
    float* colour_scale = nullptr;
    float* alpha_scale = nullptr;
    int* sectors_per_band = nullptr;
    int max_num_sectors = 0;
    int start_band = 0;
    int end_band = 0;
    
    // === Main processing loop ===
    for (int iteration = 0; iteration < 10000; ++iteration) {
        // Read audio from ALSA
        snd_pcm_sframes_t frames_read = snd_pcm_readi(pcm_handle, alsa_buffer, framesize);
        
        if (frames_read == -EPIPE) {
            snd_pcm_prepare(pcm_handle);
            continue;
        } else if (frames_read < 0) {
            std::cout << "ALSA Error: " << snd_strerror(frames_read) << std::endl;
            continue;
        } else if (frames_read != framesize) {
            continue; // Wait for full frame
        }
        
        // Convert interleaved 24-bit to float channels
        convert_interleaved_to_float_channels(alsa_buffer, mic_input, framesize, mic_channels);
        
        // === Process with array2sh (mic signals -> SH signals) ===
        array2sh_process(array2sh_handle, 
                         (const float* const*)mic_input, 
                         sh_output, 
                         mic_channels, 
                         NUM_SH_SIGNALS, 
                         framesize);
        
        // === Process with sldoa (SH signals -> DoA estimates) ===
        sldoa_analysis(sld_handle, 
                       (const float* const*)sh_output, 
                       NUM_SH_SIGNALS, 
                       framesize, 
                       1); // isPlaying = 1
        
        // Increment frame counter
        frame_counter++;
        
        // Only get display data every N frames (when sldoa has processed a full block)
        if (frame_counter >= frames_per_sldoa_update) {
            frame_counter = 0;
            sldoa_getDisplayData(sld_handle, &azi_deg, &elev_deg, &colour_scale, &alpha_scale,
                                 &sectors_per_band, &max_num_sectors, &start_band, &end_band);
        }
        
        // Calculate input level for activity detection
        float input_energy = 0.0f;
        for (int ch = 0; ch < mic_channels; ++ch) {
            for (int s = 0; s < framesize; ++s) {
                input_energy += mic_input[ch][s] * mic_input[ch][s];
            }
        }
        input_energy /= (mic_channels * framesize);
        float input_db = 10.0f * log10f(input_energy + 1e-10f);
        
        // Only update display every 4 frames (to reduce flickering and CPU)
        if (iteration % 4 != 0) continue;
        
        // === Display results ===
//        std::cout << "\033[2J\033[H"; // Clear screen
//        std::cout << "=== SAF Ambisonics Sound Source Localization ===" << std::endl;
//        std::cout << "Input Level: " << std::fixed << std::setprecision(1) << input_db << " dB" << std::endl;
//        std::cout << std::endl;
        
        // Debug: Check SH output energy
        float sh_energy = 0.0f;
        for (int ch = 0; ch < NUM_SH_SIGNALS; ++ch) {
            for (int s = 0; s < framesize; ++s) {
                sh_energy += sh_output[ch][s] * sh_output[ch][s];
            }
        }
        sh_energy /= (NUM_SH_SIGNALS * framesize);
        float sh_db = 10.0f * log10f(sh_energy + 1e-10f);
//        std::cout << "SH Output Level: " << sh_db << " dB" << std::endl;
//        std::cout << std::endl;
        
        // Show DoA estimates if audio is present
        if (input_db > -50.0f && azi_deg != nullptr && elev_deg != nullptr && alpha_scale != nullptr && sectors_per_band != nullptr) {
            std::cout << "\033[2J\033[H"; // Clear screen
            std::cout << "=== SAF Ambisonics Sound Source Localization ===" << std::endl;
            std::cout << "Input Level: " << std::fixed << std::setprecision(1) << input_db << " dB" << std::endl;
            std::cout << std::endl;
            std::cout << "SH Output Level: " << sh_db << " dB" << std::endl;
            std::cout << std::endl;
            
            std::cout << "Detected Sound Direction:" << std::endl;
            std::cout << "  Bands: " << start_band << " to " << end_band 
                      << ", max_num_sectors: " << max_num_sectors << std::endl;
            
            // Find the sector with maximum alpha (energy) across all frequency bands
            // Data layout: azi_deg[band * max_num_sectors + sector]
            float max_alpha = -1.0f;
            int best_band = start_band;
            int best_sector = 0;
            
            for (int band = start_band; band <= end_band; ++band) {
                int nSectors = sectors_per_band[band];
                for (int sector = 0; sector < nSectors; ++sector) {
                    int idx = band * max_num_sectors + sector;
                    if (alpha_scale[idx] > max_alpha) {
                        max_alpha = alpha_scale[idx];
                        best_band = band;
                        best_sector = sector;
                    }
                }
            }
            
            int best_idx = best_band * max_num_sectors + best_sector;
            
            // Debug: print a few values from the first valid band
            std::cout << "  Sectors in band " << start_band << ": " << sectors_per_band[start_band] << std::endl;
            
            // Display dominant direction
            std::cout << "  Azimuth:   " << std::setw(8) << std::setprecision(1) 
                      << azi_deg[best_idx] << " deg" << std::endl;
            std::cout << "  Elevation: " << std::setw(8) << std::setprecision(1) 
                      << elev_deg[best_idx] << " deg" << std::endl;
            std::cout << "  Alpha:     " << std::setw(8) << std::setprecision(3) 
                      << max_alpha << std::endl;
            std::cout << "  Band/Sector: " << best_band << "/" << best_sector << std::endl;
            
            // Simple ASCII compass visualization
            std::cout << std::endl << "  Compass (top view):" << std::endl;
            std::cout << "         N (0°)" << std::endl;
            std::cout << "           |" << std::endl;
            std::cout << "  W (-90°) + E (90°)" << std::endl;
            std::cout << "           |" << std::endl;
            std::cout << "       S (±180°)" << std::endl;
            
            // Show direction indicator
            float azi = azi_deg[best_idx];
            std::string direction;
            if (azi >= -22.5f && azi < 22.5f) direction = "Front";
            else if (azi >= 22.5f && azi < 67.5f) direction = "Front-Right";
            else if (azi >= 67.5f && azi < 112.5f) direction = "Right";
            else if (azi >= 112.5f && azi < 157.5f) direction = "Back-Right";
            else if (azi >= 157.5f || azi < -157.5f) direction = "Back";
            else if (azi >= -157.5f && azi < -112.5f) direction = "Back-Left";
            else if (azi >= -112.5f && azi < -67.5f) direction = "Left";
            else direction = "Front-Left";
            
            std::cout << std::endl << "  Direction: " << direction << std::endl;
            std::cout << std::endl << "Frame: " << iteration << std::flush;
            
        } else {
            //std::cout << "(No significant sound detected - make some noise!)" << std::endl;
        }
        
        //std::cout << std::endl << "Frame: " << iteration << std::flush;
    }
    
    // === Cleanup ===
    std::cout << std::endl << "Cleaning up..." << std::endl;
    
    snd_pcm_drop(pcm_handle);
    snd_pcm_close(pcm_handle);
    
    sldoa_destroy(&sld_handle);
    array2sh_destroy(&array2sh_handle);
    
    for (int i = 0; i < mic_channels; ++i) delete[] mic_input[i];
    delete[] mic_input;
    for (int i = 0; i < NUM_SH_SIGNALS; ++i) delete[] sh_output[i];
    delete[] sh_output;
    delete[] alsa_buffer;
    
    std::cout << "Done!" << std::endl;
    return 0;
}