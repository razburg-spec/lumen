#pragma once

// Settings shared by every reverb algorithm.
struct ReverbParams
{
    int mode = 0;            // 0 Hall, 1 Plate, 2 Room, 3 Chamber, 4 Bloom, 5 Cloud
    int era = 4;             // 0 60s, 1 70s, 2 80s, 3 90s, 4 Now
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
