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
        [[asset("mat")]]
        Handle material;

        [[asset("fbx", "obj", "gltf", "glb")]]
        Handle model;
    };
}
