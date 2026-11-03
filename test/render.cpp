// Offline render of the Iroi DSP chain (Filter -> Resonator -> Echo ->
// Ambience) without the UI. Controls are set directly as knob positions
// (0..1), the way MIDI CCs set them on the module.
//
//   render in.wav out.wav [key=value ...]
//   render --demo out.wav            writes a dry test source
//
// Keys: shimmer, interval, spread, tone, climb, grain, decay, spacetime, ambvol, autopan, echovol, resovol,
// cutoff (0..1 of the note range), tail (seconds of silence appended).
// Input can be 16/24/32-bit PCM or 32-bit float, mono or stereo. Anything
// not at 48 kHz is linearly resampled. Output is 32-bit float stereo.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <string>
#include <vector>

#include "Iroi.h"
#include "Clock.h"
#include "FastPowTable.h"
#include "FastLogTable.h"

static const int kSampleRate = 48000;
static const int kBlockSize = 64;

// Normally provided by OwlProgram's Patch.cpp and firmware.
AudioBuffer::~AudioBuffer() {}

void assert_failed(const char* msg, const char* location, int line)
{
    fprintf(stderr, "Assertion failed: %s (%s:%d)\n", msg, location, line);
    exit(1);
}

class StereoBuffer : public AudioBuffer
{
private:
    FloatArray channels_[2];
    int size_;

public:
    StereoBuffer(int size) : size_(size)
    {
        channels_[0] = FloatArray::create(size);
        channels_[1] = FloatArray::create(size);
    }
    ~StereoBuffer()
    {
        FloatArray::destroy(channels_[0]);
        FloatArray::destroy(channels_[1]);
    }
    FloatArray getSamples(int channel) override { return channels_[channel]; }
    int getChannels() override { return 2; }
    int getSize() override { return size_; }
    void clear() override
    {
        channels_[0].clear();
        channels_[1].clear();
    }
};

struct Audio
{
    int rate = kSampleRate;
    std::vector<float> l, r;
};

static uint32_t U32(const uint8_t* p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32_t)p[3] << 24; }
static uint16_t U16(const uint8_t* p) { return p[0] | p[1] << 8; }

static bool ReadWav(const char* path, Audio& audio)
{
    FILE* f = fopen(path, "rb");
    if (!f) return false;
    std::vector<uint8_t> d;
    uint8_t buf[65536];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) d.insert(d.end(), buf, buf + n);
    fclose(f);
    if (d.size() < 12 || memcmp(&d[0], "RIFF", 4) || memcmp(&d[8], "WAVE", 4)) return false;

    int format = 0, channels = 0, bits = 0;
    const uint8_t* data = NULL;
    size_t dataSize = 0;
    for (size_t p = 12; p + 8 <= d.size();)
    {
        uint32_t size = U32(&d[p + 4]);
        const uint8_t* body = &d[p + 8];
        if (!memcmp(&d[p], "fmt ", 4))
        {
            format = U16(body);
            channels = U16(body + 2);
            audio.rate = U32(body + 4);
            bits = U16(body + 14);
            if (format == 0xFFFE) format = U16(body + 24); // WAVE_FORMAT_EXTENSIBLE
        }
        else if (!memcmp(&d[p], "data", 4))
        {
            data = body;
            dataSize = std::min((size_t)size, d.size() - (p + 8));
        }
        p += 8 + size + (size & 1);
    }
    if (!data || channels < 1 || !((format == 1 && (bits == 16 || bits == 24 || bits == 32)) || (format == 3 && bits == 32)))
    {
        fprintf(stderr, "%s: unsupported WAV (format %d, %d bits, %d channels)\n", path, format, bits, channels);
        return false;
    }

    int bytes = bits / 8;
    size_t frames = dataSize / (bytes * channels);
    audio.l.resize(frames);
    audio.r.resize(frames);
    for (size_t i = 0; i < frames; i++)
    {
        float s[2];
        for (int c = 0; c < 2; c++)
        {
            const uint8_t* p = data + (i * channels + std::min(c, channels - 1)) * bytes;
            if (format == 3) memcpy(&s[c], p, 4);
            else if (bits == 16) s[c] = (int16_t)U16(p) / 32768.f;
            else if (bits == 24) s[c] = ((int32_t)(p[0] << 8 | p[1] << 16 | (uint32_t)p[2] << 24) >> 8) / 8388608.f;
            else s[c] = (int32_t)U32(p) / 2147483648.f;
        }
        audio.l[i] = s[0];
        audio.r[i] = s[1];
    }
    return true;
}

static bool WriteWav(const char* path, const Audio& audio)
{
    FILE* f = fopen(path, "wb");
    if (!f) return false;
    uint32_t frames = audio.l.size();
    uint32_t dataSize = frames * 8;
    auto u32 = [&](uint32_t v) { fwrite(&v, 4, 1, f); };
    auto u16 = [&](uint16_t v) { fwrite(&v, 2, 1, f); };
    fwrite("RIFF", 1, 4, f); u32(36 + dataSize); fwrite("WAVE", 1, 4, f);
    fwrite("fmt ", 1, 4, f); u32(16); u16(3); u16(2); u32(audio.rate); u32(audio.rate * 8); u16(8); u16(32);
    fwrite("data", 1, 4, f); u32(dataSize);
    for (uint32_t i = 0; i < frames; i++)
    {
        fwrite(&audio.l[i], 4, 1, f);
        fwrite(&audio.r[i], 4, 1, f);
    }
    fclose(f);
    return true;
}

static void Resample(Audio& audio)
{
    if (audio.rate == kSampleRate) return;
    fprintf(stderr, "Resampling %d Hz to %d Hz (linear)\n", audio.rate, kSampleRate);
    double step = (double)audio.rate / kSampleRate;
    size_t frames = (size_t)((audio.l.size() - 1) / step);
    Audio out;
    out.l.resize(frames);
    out.r.resize(frames);
    for (size_t i = 0; i < frames; i++)
    {
        double pos = i * step;
        size_t j = (size_t)pos;
        float t = pos - j;
        out.l[i] = audio.l[j] + (audio.l[j + 1] - audio.l[j]) * t;
        out.r[i] = audio.r[j] + (audio.r[j + 1] - audio.r[j]) * t;
    }
    audio = out;
}

// Dry source: three chords of decaying additive tones, then silence for the
// tails. Slightly detuned between channels.
static void MakeDemo(Audio& audio)
{
    const float chords[3][4] = {
        { 48, 55, 64, 71 }, // Cmaj7
        { 45, 52, 60, 67 }, // Am7
        { 41, 48, 57, 64 }, // Fmaj7
    };
    const int chordSamples = 2 * kSampleRate;
    size_t frames = 3 * chordSamples + 6 * kSampleRate;
    audio.rate = kSampleRate;
    audio.l.assign(frames, 0.f);
    audio.r.assign(frames, 0.f);
    for (int c = 0; c < 3; c++)
    {
        for (int v = 0; v < 4; v++)
        {
            float f = 440.f * powf(2.f, (chords[c][v] - 69) / 12.f);
            for (int i = 0; i < chordSamples; i++)
            {
                float t = (float)i / kSampleRate;
                float env = (1.f - expf(-t * 200.f)) * expf(-t * 2.5f);
                float s = 0;
                for (int h = 1; h <= 6; h++)
                {
                    s += sinf(2.f * M_PI * f * h * t) * expf(-t * h * 1.5f) / h;
                }
                float sr = 0;
                for (int h = 1; h <= 6; h++)
                {
                    sr += sinf(2.f * M_PI * f * 1.002f * h * t) * expf(-t * h * 1.5f) / h;
                }
                audio.l[c * chordSamples + i] += s * env * 0.12f;
                audio.r[c * chordSamples + i] += sr * env * 0.12f;
            }
        }
    }
}

// Same startup values as Ui's constructor, with the filter fully open (it is
// also the input VCA) and only the ambience mixed in.
static void InitCtrls(PatchCtrls& c)
{
    c = {};
    c.filterMode = 0.f;
    c.filterPosition = 0.f;
    c.modType = 0.f;
    c.resonatorDissonance = 0.f;
    c.echoFilter = 0.5f;
    c.ambienceAutoPan = 0.f;
    c.ambienceShimmer = 0.f;
    c.ambienceShimmerInterval = kShimmerIntervalDefault;
    c.ambienceShimmerSpread = 0.f;
    c.ambienceShimmerTone = kShimmerToneDefault;
    c.ambienceShimmerClimb = kShimmerClimbDefault;
    c.ambienceShimmerGrain = kShimmerGrainDefault;
    c.filterCutoffModAmount = 0.5f;
    c.filterCutoffCvAmount = 1.f;
    c.resonatorTuneCvAmount = 1.f;
    c.echoDensityCvAmount = 1.f;
    c.ambienceSpacetimeCvAmount = 1.f;
    c.filterCutoffRndAmount = 0.5f;
    c.filterResonanceRndAmount = 0.5f;
    c.resonatorTuneRndAmount = 0.5f;
    c.resonatorFeedbackRndAmount = 0.5f;
    c.echoDensityRndAmount = 0.5f;
    c.echoRepeatsRndAmount = 0.5f;
    c.ambienceDecayRndAmount = 0.5f;
    c.ambienceSpacetimeRndAmount = 0.5f;

    c.filterVol = 1.f;
    c.filterCutoff = 127.f;
    c.filterResonance = 0.f;
    c.resonatorVol = 0.f;
    c.resonatorTune = 0.5f;
    c.resonatorFeedback = 0.5f;
    c.echoVol = 0.f;
    c.echoRepeats = 0.5f;
    c.echoDensity = 0.5f;
    c.ambienceVol = 0.6f;
    c.ambienceDecay = 0.8f;
    c.ambienceSpacetime = 0.75f;
    c.modLevel = 0.f;
    c.modSpeed = 0.5f;
}

static bool SetCtrl(PatchCtrls& c, float& tail, const char* arg)
{
    const char* eq = strchr(arg, '=');
    if (!eq) return false;
    std::string key(arg, eq - arg);
    float v = atof(eq + 1);
    if (key == "shimmer") c.ambienceShimmer = v;
    else if (key == "interval") c.ambienceShimmerInterval = v;
    else if (key == "spread") c.ambienceShimmerSpread = v;
    else if (key == "tone") c.ambienceShimmerTone = v;
    else if (key == "climb") c.ambienceShimmerClimb = v;
    else if (key == "grain") c.ambienceShimmerGrain = v;
    else if (key == "decay") c.ambienceDecay = v;
    else if (key == "spacetime") c.ambienceSpacetime = v;
    else if (key == "ambvol") c.ambienceVol = v;
    else if (key == "autopan") c.ambienceAutoPan = v;
    else if (key == "echovol") c.echoVol = v;
    else if (key == "resovol") c.resonatorVol = v;
    else if (key == "cutoff") c.filterCutoff = v * 127.f;
    else if (key == "tail") tail = v;
    else return false;
    return true;
}

int main(int argc, char** argv)
{
    if (argc == 3 && !strcmp(argv[1], "--demo"))
    {
        Audio demo;
        MakeDemo(demo);
        return WriteWav(argv[2], demo) ? 0 : 1;
    }
    if (argc < 3)
    {
        fprintf(stderr, "usage: %s in.wav out.wav [key=value ...]\n       %s --demo out.wav\n", argv[0], argv[0]);
        return 1;
    }

    PatchCtrls ctrls;
    PatchCvs cvs = {};
    PatchState state = {};
    float tail = 0.f;
    InitCtrls(ctrls);
    for (int i = 3; i < argc; i++)
    {
        if (!SetCtrl(ctrls, tail, argv[i]))
        {
            fprintf(stderr, "Unknown setting %s\n", argv[i]);
            return 1;
        }
    }

    Audio audio;
    if (!ReadWav(argv[1], audio))
    {
        fprintf(stderr, "Can't read %s\n", argv[1]);
        return 1;
    }
    Resample(audio);
    audio.l.resize(audio.l.size() + (size_t)(tail * kSampleRate), 0.f);
    audio.r.resize(audio.r.size() + (size_t)(tail * kSampleRate), 0.f);

    // The firmware installs these on the module (setSystemTables), these are
    // OwlProgram's fallbacks.
    fast_pow_set_table(fast_pow_table, fast_pow_table_size);
    fast_log_set_table(fast_log_table, fast_log_table_size);

    state.sampleRate = kSampleRate;
    state.blockSize = kBlockSize;
    state.blockRate = (float)kSampleRate / kBlockSize;
    state.inputLevel = FloatArray::create(kBlockSize);
    state.outputLevel = FloatArray::create(kBlockSize);
    state.efModLevel = FloatArray::create(kBlockSize);
    state.outLevel = 1.f;
    state.randomSlew = kRandomSlewSamples;

    Clock* clock = Clock::create(&ctrls, &state);
    Iroi* iroi = Iroi::create(&ctrls, &cvs, &state);
    StereoBuffer buffer(kBlockSize);
    FloatArray left = buffer.getSamples(LEFT_CHANNEL);
    FloatArray right = buffer.getSamples(RIGHT_CHANNEL);

    size_t frames = audio.l.size() / kBlockSize * kBlockSize;
    float peak = 0.f;
    for (size_t p = 0; p < frames; p += kBlockSize)
    {
        for (int i = 0; i < kBlockSize; i++)
        {
            left[i] = audio.l[p + i];
            right[i] = audio.r[p + i];
        }
        clock->Process();
        iroi->Process(buffer);
        for (int i = 0; i < kBlockSize; i++)
        {
            audio.l[p + i] = left[i];
            audio.r[p + i] = right[i];
            peak = std::max(peak, std::max(fabsf(left[i]), fabsf(right[i])));
        }
    }
    audio.l.resize(frames);
    audio.r.resize(frames);

    Iroi::destroy(iroi);
    Clock::destroy(clock);

    if (!WriteWav(argv[2], audio))
    {
        fprintf(stderr, "Can't write %s\n", argv[2]);
        return 1;
    }
    printf("%s: %.1f s, peak %.2f (%.1f dBFS)%s\n", argv[2], (float)frames / kSampleRate, peak, 20.f * log10f(peak + 1e-9f), peak > 1.f ? " - would clip on the module" : "");
    return 0;
}
