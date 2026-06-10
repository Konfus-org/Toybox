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
        [[prop]]
        Handle material;

        [[prop]]
        Handle model;
    };
}
