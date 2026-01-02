#include "hw.h"

// Allocate SDRAM buffers
float DSY_SDRAM_BSS Hardware::buffer_a[LOOPER_MAX_SAMPLES];
float DSY_SDRAM_BSS Hardware::buffer_b[LOOPER_MAX_SAMPLES];

void Hardware::Init()
{
    seed.Init();
    seed.SetAudioBlockSize(4);
    sample_rate = seed.AudioSampleRate();

    // --- Encoders ---
    encoder1.Init(seed.GetPin(5), seed.GetPin(4), seed.GetPin(6), seed.AudioCallbackRate());
    encoder2.Init(seed.GetPin(8), seed.GetPin(7), seed.GetPin(9), seed.AudioCallbackRate());

    // --- Button 1 (Looper) ---
    button1.Init(seed.GetPin(1), seed.AudioCallbackRate());
}

void Hardware::ProcessControls()
{
    encoder1.Debounce();
    encoder2.Debounce();
    button1.Debounce();
}