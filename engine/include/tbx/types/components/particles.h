#pragma once

namespace tbx
{
    [[serializable]];
    struct ParticlesSettings
    {
        // TODO: fill out settings
    };

    [[serializable]];
    struct Particles
    {
        Handle material;

        Handle model;
    };
}
