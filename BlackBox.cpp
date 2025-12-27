#include "config.h"
#include "hw.h"
#include "screen.h"
// #include "processing.h" 

static Hardware   g_hw;
static Screen     g_screen;

// Audio Callback - Runs at high frequency (approx 1kHz - 48kHz depending on block size)
void AudioCallback(AudioHandle::InputBuffer  in,
                   AudioHandle::OutputBuffer out,
                   size_t                    size)
{
    // 1. Process Hardware here for accurate timing/debouncing
    g_hw.ProcessTestControls();

    // 2. Audio Pass-through Test
    for(size_t i = 0; i < size; i++)
    {
        // Pass input directly to output
        out[0][i] = in[0][i];
        out[1][i] = in[1][i];
    }
}

int main(void)
{
    // Initialize Hardware
    g_hw.Init();
    g_screen.Init(g_hw.seed);

    // Start the Audio Callback
    g_hw.seed.StartAudio(AudioCallback);

    while(1)
    {
        // Update Screen with Test UI (Low priority, ~30fps)
        g_screen.DrawHardwareTest(g_hw);
        
        // Delay to throttle screen updates
        System::Delay(33);
    }
}