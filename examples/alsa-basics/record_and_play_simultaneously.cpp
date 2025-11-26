#include "alsa/asoundlib.h"
#include <cstring>
#include <iostream>

#define PCM_REC_DEVICE "hw:3,0"
#define PCM_PLAY_DEVICE "hw:1,0"
#define SAMPLE_RATE 44100
#define REC_CHANNELS 1
#define PLAY_CHANNELS 2
#define DURATION_SECONDS 2
#define BUFFER_SIZE 4096

int main() {
  snd_pcm_t *rec_handle, *play_handle;
  snd_pcm_hw_params_t *rec_params, *play_params;
  unsigned int rate = SAMPLE_RATE;
  int err;
  int total_frames = SAMPLE_RATE * DURATION_SECONDS;
  char *buffer = new char[BUFFER_SIZE];

  // Open PCM device for recording
  if ((err = snd_pcm_open(&rec_handle, PCM_REC_DEVICE, SND_PCM_STREAM_CAPTURE,
                          0)) < 0) {
    std::cerr << "Error opening recording device: " << snd_strerror(err)
              << std::endl;
    return 1;
  }
  std::cout << "Recording from " << PCM_REC_DEVICE << " for "
            << DURATION_SECONDS << " seconds..." << std::endl;

  // Set recording hardware parameters
  snd_pcm_hw_params_malloc(&rec_params);
  snd_pcm_hw_params_any(rec_handle, rec_params);
  snd_pcm_hw_params_set_access(rec_handle, rec_params,
                               SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(rec_handle, rec_params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(rec_handle, rec_params, REC_CHANNELS);
  snd_pcm_hw_params_set_rate_near(rec_handle, rec_params, &rate, 0);
  snd_pcm_hw_params(rec_handle, rec_params);
  snd_pcm_hw_params_free(rec_params);
  snd_pcm_start(rec_handle);

  // Record audio (mono)
  char *record_buffer = new char[total_frames * REC_CHANNELS * 2];
  int offset = 0;
  int remaining = total_frames;

  while (remaining > 0) {
    int frames_to_read =
        (remaining > BUFFER_SIZE / 4) ? BUFFER_SIZE / 4 : remaining;
    int frames_read = snd_pcm_readi(rec_handle, buffer, frames_to_read);

    if (frames_read < 0) {
      std::cerr << "Error reading: " << snd_strerror(frames_read) << std::endl;
      break;
    }

    std::memcpy(record_buffer + offset, buffer, frames_read * REC_CHANNELS * 2);
    offset += frames_read * REC_CHANNELS * 2;
    remaining -= frames_read;
  }

  snd_pcm_close(rec_handle);
  std::cout << "Recording complete." << std::endl;

  // Convert mono to stereo
  char *stereo_buffer = new char[total_frames * PLAY_CHANNELS * 2];
  for (int i = 0; i < total_frames; i++) {
    int16_t mono_sample = *(int16_t *)(record_buffer + i * 2);
    *(int16_t *)(stereo_buffer + i * PLAY_CHANNELS * 2) = mono_sample;
    *(int16_t *)(stereo_buffer + i * PLAY_CHANNELS * 2 + 2) = mono_sample;
  }

  // Open PCM device for playback
  if ((err = snd_pcm_open(&play_handle, PCM_PLAY_DEVICE,
                          SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    std::cerr << "Error opening playback device: " << snd_strerror(err)
              << std::endl;
    return 1;
  }
  std::cout << "Playing back on " << PCM_PLAY_DEVICE << "..." << std::endl;

  // Set playback hardware parameters
  snd_pcm_hw_params_malloc(&play_params);
  snd_pcm_hw_params_any(play_handle, play_params);
  snd_pcm_hw_params_set_access(play_handle, play_params,
                               SND_PCM_ACCESS_RW_INTERLEAVED);
  snd_pcm_hw_params_set_format(play_handle, play_params, SND_PCM_FORMAT_S16_LE);
  snd_pcm_hw_params_set_channels(play_handle, play_params, PLAY_CHANNELS);
  snd_pcm_hw_params_set_rate_near(play_handle, play_params, &rate, 0);
  snd_pcm_hw_params(play_handle, play_params);
  snd_pcm_hw_params_free(play_params);

  // Playback audio
  offset = 0;
  remaining = total_frames;

  while (remaining > 0) {
    int frames_to_write =
        (remaining > BUFFER_SIZE / 4) ? BUFFER_SIZE / 4 : remaining;
    int frames_written =
        snd_pcm_writei(play_handle, stereo_buffer + offset * PLAY_CHANNELS * 2,
                       frames_to_write);

    if (frames_written < 0) {
      std::cerr << "Error writing: " << snd_strerror(frames_written)
                << std::endl;
      break;
    }

    offset += frames_written;
    remaining -= frames_written;
  }

  snd_pcm_drain(play_handle);
  snd_pcm_close(play_handle);
  std::cout << "Playback complete." << std::endl;

  delete[] buffer;
  delete[] record_buffer;
  delete[] stereo_buffer;
  return 0;
}
