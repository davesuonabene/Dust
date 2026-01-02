#include "config.h"
#include "hw.h"
#include "screen.h"
#include "processing.h" 

static Hardware   g_hw;
static Screen     g_screen;
static Processing g_proc;

void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    // 1. Process Hardware
    g_hw.ProcessControls();
    
    // 2. Process UI Logic
    g_proc.Controls(g_hw);

    // 3. Audio Processing
    float outl, outr, inl, inr;
    for(size_t i = 0; i < size; i++)
    {
        inl = in[0][i];
        inr = in[1][i];

        g_proc.GetSample(outl, outr, inl, inr);

        out[0][i] = outl;
        out[1][i] = outr;
    }
}

int main(void)
{
    // Initialize
    g_hw.Init();
    g_proc.Init(g_hw);
    g_screen.Init(g_hw.seed);

    // Start Audio
    g_hw.seed.StartAudio(AudioCallback);

    while(1)
    {
        // Update Screen
        g_screen.DrawStatus(g_proc, g_hw);
        
        // Throttling
        System::Delay(33);
    }
}