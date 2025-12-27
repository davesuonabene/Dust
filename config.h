#pragma once

// Set max buffer time to 2 seconds @ 48kHz
#define MAX_BUFFER_SAMPLES static_cast<size_t>(48000 * 2.0f)

// Max grains to play simultaneously
#define MAX_GRAINS 8