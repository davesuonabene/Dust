#include "hw.h"

// Definition of the static loop buffers
float DSY_SDRAM_BSS Hardware::buffer_a[LOOPER_MAX_SAMPLES];
float DSY_SDRAM_BSS Hardware::buffer_b[LOOPER_MAX_SAMPLES];

void Hardware::Init()
{
    seed.Init();
    seed.SetAudioBlockSize(4);
    sample_rate = seed.AudioSampleRate();

    // --- ADC Configuration ---
    // No Knobs/ADC in new hardware for now

    // --- Encoders ---
    // Encoder 1: Pins 4, 5, 6 (A, B, Click)
    encoder1.Init(seed.GetPin(4), seed.GetPin(5), seed.GetPin(6), seed.AudioCallbackRate());
    
    // Encoder 2: Pins 7, 8, 9 (A, B, Click)
    encoder2.Init(seed.GetPin(7), seed.GetPin(8), seed.GetPin(9), seed.AudioCallbackRate());

    // --- Buttons ---
    // Pins 1, 2, 3
    button1.Init(seed.GetPin(1), seed.AudioCallbackRate());
    button2.Init(seed.GetPin(2), seed.AudioCallbackRate());
    button3.Init(seed.GetPin(3), seed.AudioCallbackRate());

    // --- Looper Init ---
    Reset();
}

void Hardware::ProcessTestControls()
{
    encoder1.Debounce();
    encoder2.Debounce();
    button1.Debounce();
    button2.Debounce();
    button3.Debounce();

    enc1_count += encoder1.Increment();
    enc2_count += encoder2.Increment();
}