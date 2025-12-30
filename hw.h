#pragma once
#include "daisy_seed.h"
#include "daisysp.h"

using namespace daisy;
using namespace daisysp;

// 20 seconds @ 48kHz = 960,000 samples
#define LOOPER_MAX_SAMPLES 960000

struct Hardware
{
    DaisySeed seed;
    
    // --- Hardware Configuration ---
    // Encoder 1: Pins 4, 5, 6 (Main Control: Nav/Edit)
    Encoder   encoder1;
    // Encoder 2: Pins 7, 8, 9 (Page Selector)
    Encoder   encoder2;
    
    // Buttons: Pins 1, 2, 3 (Reserved/Unused)
    Switch    button1;
    Switch    button2;
    Switch    button3;

    float     sample_rate;
    float     callback_rate;

    // --- Looper Data ---
    static float DSY_SDRAM_BSS buffer_a[LOOPER_MAX_SAMPLES];
    static float DSY_SDRAM_BSS buffer_b[LOOPER_MAX_SAMPLES];

    float* active_buffer = nullptr;
    float* rec_buffer    = nullptr;
    
    enum LooperMode { LP_EMPTY, LP_RECORDING, LP_PLAYING, LP_STOPPED };
    LooperMode looper_mode = LP_EMPTY;
    
    uint32_t loop_length = 0;
    uint32_t play_pos    = 0;
    uint32_t rec_pos     = 0;

    void Init();
    void ProcessControls(); 
    
    void SwitchToNewLoop()
    {
        active_buffer = rec_buffer;
        loop_length   = rec_pos;
        play_pos      = 0;
        if (active_buffer == buffer_a) rec_buffer = buffer_b;
        else                           rec_buffer = buffer_a;
    }
    
    void Reset()
    {
        looper_mode = LP_EMPTY;
        loop_length = 0;
        play_pos    = 0;
        rec_pos     = 0;
        active_buffer = buffer_a;
        rec_buffer    = buffer_b;
    }
};