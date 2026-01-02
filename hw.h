#pragma once
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

// 20 seconds @ 48kHz
#define LOOPER_MAX_SAMPLES 960000

struct Hardware
{
    DaisySeed seed;
    
    // --- Hardware Configuration ---
    Encoder   encoder1; // Nav/Edit
    Encoder   encoder2; // Page
    Switch    button1;  // Looper Control

    float     sample_rate;

    // --- Looper Data (SDRAM) ---
    static float DSY_SDRAM_BSS buffer_a[LOOPER_MAX_SAMPLES];
    static float DSY_SDRAM_BSS buffer_b[LOOPER_MAX_SAMPLES];

    void Init();
    void ProcessControls(); 
};