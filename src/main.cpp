#include <alsa/asoundlib.h>

typedef struct WAVHeader
{
    char riff[4]; // "RIFF"
    uint32_t chunkSize;
    char wave[4]; // "WAVE"
    char fmt[4];  // "fmt "
    uint32_t subchunk1Size;
    uint16_t audioFormat;
    uint16_t numChannels;
    uint32_t sampleRate;
    uint32_t byteRate;
    uint16_t blockAlign;
    uint16_t bitsPerSample;
    char data[4]; // "data"
    uint32_t subchunk2Size;
} WAVHeader;

void write_wav_header(FILE *file, int sample_rate, int num_channels, int bits_per_sample, int data_size)
{
    WAVHeader header;
    memcpy(header.riff, "RIFF", 4);
    header.chunkSize = 36 + data_size;
    memcpy(header.wave, "WAVE", 4);
    memcpy(header.fmt, "fmt ", 4);
    header.subchunk1Size = 16;
    header.audioFormat = 1; // PCM
    header.numChannels = num_channels;
    header.sampleRate = sample_rate;
    header.byteRate = sample_rate * num_channels * bits_per_sample / 8;
    header.blockAlign = num_channels * bits_per_sample / 8;
    header.bitsPerSample = bits_per_sample;
    memcpy(header.data, "data", 4);
    header.subchunk2Size = data_size;

    fwrite(&header, sizeof(WAVHeader), 1, file);
}

/// record for 5 seconds and save to /tmp/flowy/output.wav
/// @returns non-zero on error
int test_record_to_wav()
{
    const char *device_name = "default";               // ALSA device name
    unsigned int rate = 44100;                         // Sample rate
    int channels = 2;                                  // Number of channels
    int buffer_frames = 128;                           // Buffer size in frames
    int duration = 5;                                  // Recording duration in seconds
    const char *output_file = "/tmp/flowy/output.wav"; // Output WAV file

    snd_pcm_t *capture_handle;
    snd_pcm_hw_params_t *hw_params;
    FILE *file;
    char *buffer;
    int err;

    // Open the audio device
    if ((err = snd_pcm_open(&capture_handle, device_name, SND_PCM_STREAM_CAPTURE, 0)) < 0)
    {
        fprintf(stderr, "Cannot open audio device %s (%s)\n", device_name, snd_strerror(err));
        return 1;
    }

    // Allocate hardware parameters object
    snd_pcm_hw_params_malloc(&hw_params);
    snd_pcm_hw_params_any(capture_handle, hw_params);

    // Set hardware parameters
    snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, 0);
    snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels);
    snd_pcm_hw_params(capture_handle, hw_params);

    // Allocate buffer
    buffer = (char *)malloc(buffer_frames * channels * 2); // 2 bytes/sample

    // Open output file
    file = fopen(output_file, "wb");
    if (!file)
    {
        fprintf(stderr, "Cannot open output file %s\n", output_file);
        return 1;
    }

    // Write WAV header
    write_wav_header(file, rate, channels, 16, 0);

    // Record audio
    int total_frames = 0;
    for (int i = 0; i < duration * rate / buffer_frames; ++i)
    {
        if ((err = snd_pcm_readi(capture_handle, buffer, buffer_frames)) < 0)
        {
            if (err == -EPIPE)
            {
                snd_pcm_prepare(capture_handle);
            }
            else
            {
                fprintf(stderr, "Read error: %s\n", snd_strerror(err));
                break;
            }
        }
        else
        {
            fwrite(buffer, channels * 2, buffer_frames, file);
            total_frames += buffer_frames;
        }
    }

    // Update WAV header with actual data size
    fseek(file, 0, SEEK_SET);
    write_wav_header(file, rate, channels, 16, total_frames * channels * 2);

    // Clean up
    fclose(file);
    free(buffer);
    snd_pcm_close(capture_handle);

    fprintf(stderr, "Recording saved to %s\n", output_file);

    return 0;
}

// record audio and playback in default output
int test_loopback_audio()
{
    const char *capture_device = "default";  // ALSA capture device
    const char *playback_device = "default"; // ALSA playback device
    unsigned int rate = 44100;               // Sample rate
    int channels = 2;                        // Number of channels
    int buffer_frames = 128;                 // Buffer size in frames
    int duration = 10;                       // Recording duration in seconds

    snd_pcm_t *capture_handle;
    snd_pcm_t *playback_handle;
    snd_pcm_hw_params_t *hw_params;
    char *buffer;
    int err;

    // Open the capture audio device
    if ((err = snd_pcm_open(&capture_handle, capture_device, SND_PCM_STREAM_CAPTURE, 0)) < 0)
    {
        fprintf(stderr, "Cannot open audio device %s (%s)\n", capture_device, snd_strerror(err));
        return 1;
    }

    // Open the playback audio device
    if ((err = snd_pcm_open(&playback_handle, playback_device, SND_PCM_STREAM_PLAYBACK, 0)) < 0)
    {
        fprintf(stderr, "Cannot open audio device %s (%s)\n", playback_device, snd_strerror(err));
        return 1;
    }

    if ((err = snd_pcm_set_params(playback_handle, SND_PCM_FORMAT_S16_LE, SND_PCM_ACCESS_RW_INTERLEAVED, channels, rate, 1, 500000)) < 0)
    {
        fprintf(stderr, "Playback open error: %s\n", snd_strerror(err));
        return 1;
    }

    // Allocate hardware parameters object
    snd_pcm_hw_params_malloc(&hw_params);
    snd_pcm_hw_params_any(capture_handle, hw_params);

    // Set hardware parameters
    snd_pcm_hw_params_set_access(capture_handle, hw_params, SND_PCM_ACCESS_RW_INTERLEAVED);
    snd_pcm_hw_params_set_format(capture_handle, hw_params, SND_PCM_FORMAT_S16_LE);
    snd_pcm_hw_params_set_rate_near(capture_handle, hw_params, &rate, 0);
    snd_pcm_hw_params_set_channels(capture_handle, hw_params, channels);
    snd_pcm_hw_params(capture_handle, hw_params);

    // Allocate buffer
    buffer = (char *)malloc(buffer_frames * channels * 2); // 2 bytes/sample

    // Start capture and playback
    for (int i = 0; i < duration * rate / buffer_frames; ++i)
    {
        if ((err = snd_pcm_readi(capture_handle, buffer, buffer_frames)) < 0)
        {
            if (err == -EPIPE)
            {
                snd_pcm_prepare(capture_handle);
            }
            else
            {
                fprintf(stderr, "Read error: %s\n", snd_strerror(err));
                break;
            }
        }
        else
        {
            err = snd_pcm_writei(playback_handle, buffer, buffer_frames);
            if (err < 0)
            {
                if (err == -EPIPE)
                {
                    snd_pcm_prepare(playback_handle);
                }
                else
                {
                    fprintf(stderr, "Write error: %s\n", snd_strerror(err));
                    break;
                }
            }
        }
    }

    free(buffer);
    snd_pcm_close(capture_handle);
    snd_pcm_close(playback_handle);

    fprintf(stderr, "Loopback test completed\n");

    return 0;
}

int main(int argc, char *argv[])
{
    if (test_record_to_wav() != 0)
    {
        fprintf(stderr, "Recording failed\n");
        return 1;
    }

    if (test_loopback_audio() != 0)
    {
        fprintf(stderr, "Loopback test failed\n");
        return 1;
    }

    return 0;
}
