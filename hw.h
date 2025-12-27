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
    
    // --- New Hardware Configuration ---
    // Encoder 1: Pins 4, 5, 6
    Encoder   encoder1;
    // Encoder 2: Pins 7, 8, 9
    Encoder   encoder2;
    // Buttons: Pins 1, 2, 3
    Switch    button1;
    Switch    button2;
    Switch    button3;

    // --- Testing Variables ---
    int enc1_count = 0;
    int enc2_count = 0;

    float     sample_rate;

    // --- Looper Data ---
    // Double Buffering in SDRAM
    static float DSY_SDRAM_BSS buffer_a[LOOPER_MAX_SAMPLES];
    static float DSY_SDRAM_BSS buffer_b[LOOPER_MAX_SAMPLES];

    // Pointers to the current buffers
    float* active_buffer = nullptr; // The buffer being played (Old loop)
    float* rec_buffer    = nullptr; // The buffer being written (New loop)
    
    enum LooperMode { LP_EMPTY, LP_RECORDING, LP_PLAYING, LP_STOPPED };
    LooperMode looper_mode = LP_EMPTY;
    
    uint32_t loop_length = 0; // Length of the active loop
    uint32_t play_pos    = 0; // Read head position
    uint32_t rec_pos     = 0; // Write head position

    void Init();
    
    // Process controls for the test mode (counters & debouncing)
    void ProcessTestControls(); 
    
    // Helper to swap buffers after recording
    void SwitchToNewLoop()
    {
        active_buffer = rec_buffer;
        loop_length   = rec_pos;
        play_pos      = 0;
        
        // Next recording will use the other buffer
        if (active_buffer == buffer_a) rec_buffer = buffer_b;
        else                           rec_buffer = buffer_a;
    }
    
    void Reset()
    {
        looper_mode = LP_EMPTY;
        loop_length = 0;
        play_pos    = 0;
        rec_pos     = 0;
        // Reset pointers defaults
        active_buffer = buffer_a;
        rec_buffer    = buffer_b;
    }
};