#pragma once

// Settings shared by every reverb algorithm.
struct ReverbParams
{
    int mode = 0;            // 0 Hall, 1 Plate, 2 Room, 3 Chamber, 4 Bloom, 5 Cloud,
                             // 6 60s Chamber, 7 60s Mirror, 8 90s Hall, 9 90s Concert
    int color = 1;           // 0 Now, 1 80s, 2 70s, 3 60s, 4 90s (modes 0-5 only: modes 6-9 have their own voicing)
    float preDelayMs = 20.0f;
    float decaySeconds = 2.5f;
    float size = 0.6f;       // 0..1
    float diffusion = 0.75f; // 0..1
    float modRateHz = 0.5f;
    float modDepth = 0.25f;  // 0..1
    float highCutHz = 9000.0f;
    float lowCutHz = 100.0f;
    float width = 1.0f;      // 0..1
    float levelGain = 1.0f;
};
