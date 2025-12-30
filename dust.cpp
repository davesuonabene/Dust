#include "config.h"
#include "hw.h"
#include "screen.h"
#include "processing.h" 

static Hardware   g_hw;
static Screen     g_screen;
static Processing g_proc;

// Audio Callback - Runs at high frequency
void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    // 1. Process Hardware (Debounce)
    g_hw.ProcessControls();

    // 2. Audio Processing
    for(size_t i = 0; i < size; i++)
    {
        float outl, outr;
        g_proc.GetSample(outl, outr, in[0][i], in[1][i]);
        out[0][i] = outl;
        out[1][i] = outr;
    }
}

int main(void)
{
    // Initialize Hardware
    g_hw.Init();
    g_proc.Init(g_hw);
    g_screen.Init(g_hw.seed);

    // Start the Audio Callback
    g_hw.seed.StartAudio(AudioCallback);

    while(1)
    {
        // Handle UI Logic (Navigation, Mapping)
        g_proc.Controls(g_hw);
        
        // Update Screen
        g_screen.DrawStatus(g_proc, g_hw);
        
        // Delay to throttle screen updates (approx 30Hz)
        System::Delay(33);
    }
}